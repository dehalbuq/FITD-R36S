#!/usr/bin/env python3
"""Generate PortMaster metadata. Never synthesize a fake gameplay screenshot."""
import json
import os
import shutil
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1])
pkg = Path(sys.argv[2])
game = pkg / "fitdr36s"
assert (game / "fitd.aarch64").is_file(), "ARM64 binary missing"
assert (pkg / "FITD-R36S-ARM64.sh").is_file(), "Launcher missing"

meta = json.loads((root / "v2/port/port-release.json").read_text())
meta["name"] = "fitdr36s.zip"
meta["items"] = ["FITD-R36S-ARM64.sh", "fitdr36s"]
meta["attr"].update({
    "title": "Alone in the Dark (FITD)",
    "desc": "Experimental open-source FITD engine port for Alone in the Dark (1992). Original retail game files are required and not distributed.",
    "inst": "Copy original Alone in the Dark 1 game files to fitdr36s/data/. Requires ARM64 firmware with functional SDL3/KMSDRM and OpenGL ES 3.",
    "arch": ["aarch64"],
    "rtr": False,
    "exp": True,
    "availability": "full",
    # Ubuntu 22.04 baseline; real required symbol versions are audited by build.
    "min_glibc": "2.35",
})
(pkg / "port.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n")

screenshot_src = root / "v2/port/assets/screenshot.png"
cover_src = root / "v2/port/assets/cover.png"
if screenshot_src.is_file():
    with Image.open(screenshot_src) as img:
        assert img.width >= 640 and img.height >= 480 and img.width * 3 == img.height * 4, (
            "Gameplay screenshot must be 4:3 and at least 640x480"
        )
    shutil.copy2(screenshot_src, pkg / "screenshot.png")
    shutil.copy2(screenshot_src, game / "screenshot.png")
else:
    # Submission is blocked until a REAL in-game capture is provided.
    (pkg / "SUBMISSION_BLOCKERS.txt").write_text(
        "Missing v2/port/assets/screenshot.png: genuine 4:3 gameplay screenshot "
        "(minimum 640x480) required by PortMaster. This is a TEST PACKAGE only.\n"
    )
    if os.getenv("PORTMASTER_SUBMISSION") == "1":
        raise SystemExit("Official submission blocked: genuine gameplay screenshot missing")

if cover_src.is_file():
    shutil.copy2(cover_src, pkg / "cover.png")
    shutil.copy2(cover_src, game / "cover.png")

xml = ET.Element("gameList")
g = ET.SubElement(xml, "game")
fields = {
    "path": "./FITD-R36S-ARM64.sh",
    "name": "Alone in the Dark (FITD)",
    "desc": "Community port of the open-source FITD engine for Alone in the Dark (1992). Original commercial data is not included. Experimental ARM64/GLES3 compatibility.",
    "releasedate": "19920101T000000",
    "developer": "Infogrames (original game); yaz0r/jmimu (FITD engine)",
    "publisher": "Infogrames (original game)",
    "genre": "Survival Horror / Adventure",
    "image": "./fitdr36s/cover.png" if cover_src.is_file() else "./fitdr36s/screenshot.png",
}
for name, value in fields.items():
    ET.SubElement(g, name).text = value
ET.indent(xml, space="  ")
ET.ElementTree(xml).write(pkg / "gameinfo.xml", encoding="utf-8", xml_declaration=True)

(pkg / "README.md").write_text("""# Alone in the Dark - FITD engine for PortMaster (experimental)

Thanks to **yaz0r** and **jmimu** for the open-source FITD engine, and
the original developers at Infogrames for Alone in the Dark (1992).
Port and RK3326 fixes: **dehalbuq**, with acknowledgements to ReHaunted.

## Requirements
* Linux AArch64/ARM64 with working SDL3 KMSDRM and OpenGL ES 3.
* Original legally obtained Alone in the Dark 1 game data: put in fitdr36s/data.
* No commercial game assets are distributed.
* Save games: back up SAVE*.ITD before replacing existing installations.

## Controls
* Directional pad: movement; gamepad face buttons: game actions.
* Start + Select held: emergency quit when supported by firmware/PortMaster.

## Technical notes
* Optimized for Cortex-A35, 60 FPS maximum, VSync disabled by default.
* Uses the same frame-driven gameplay timing as tested Debug versions.
* This timing can cause acceleration in some game systems at high FPS.
* SDL3/KMSDRM GLES3 not yet validated across other major CFWs.
* Older firmware may have incompatible glibc, EGL/GBM or drivers.
* This port has not been approved by PortMaster and is not officially affiliated.

## Source
https://github.com/dehalbuq/FITD-R36S

## Testing and legal
Read fitdr36s/LEIA-ME-LEGAL.txt and fitdr36s/licenses/LICENSE.FITD.txt.
A real 640x480 4:3 gameplay screenshot is mandatory for submission.
Never use a title-card or generated illustration as the official screenshot.
""")
print("PortMaster staging complete; submission image present:", screenshot_src.is_file())
