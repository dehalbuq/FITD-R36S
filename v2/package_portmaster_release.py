#!/usr/bin/env python3
"""Create PortMaster metadata and cover inside the built ARM64 package."""
import json, sys
from pathlib import Path
from PIL import Image, ImageOps, ImageDraw, ImageFont
import xml.etree.ElementTree as ET
root=Path(sys.argv[1])
pkg=Path(sys.argv[2])
game=pkg/"fitdr36s"
assert (game/"fitd.aarch64").is_file(), "Binary must be built before packaging"
meta=json.loads((root/"v2/port/port-release.json").read_text())
meta["name"]="fitdr36s-aarch64-safe-release.zip"
meta["items"]=["FITD-R36S-ARM64.sh","fitdr36s/"]
meta["attr"]["title"]="Alone in the Dark - FITD R36S ARM64"
meta["attr"]["desc"]="Experimental open-source engine port for Alone in the Dark 1992 on R36S RK3326; original game data not included. No official PortMaster endorsement."
meta["attr"]["inst"]="Copy your legally owned original AITD 1 game files to fitdr36s/data/ before running the launcher."
meta["attr"]["arch"]=["aarch64"]
meta["attr"]["rtr"]=False
meta["attr"]["exp"]=True
(pkg/"port.json").write_text(json.dumps(meta,ensure_ascii=False,indent=2)+"\n")
(game/"port.json").write_text(json.dumps(meta,ensure_ascii=False,indent=2)+"\n")
rootxml=ET.Element("gameList")
g=ET.SubElement(rootxml,"game")
values={
"path":"./FITD-R36S-ARM64.sh",
"name":"Alone in the Dark (FITD R36S)",
"desc":"Motor FITD para Alone in the Dark (1992), adaptado por dehalbuq para RK3326 ARM64 com correcoes de gameplay. Release experimental sem debug, renderizacao ate 60 FPS (sem garantia), VSync desativado e temporizacao compativel com a Debug anterior. Nao inclui dados comerciais do jogo. Projeto nao oficial.",
"releasedate":"19920101T000000",
"developer":"Infogrames (jogo original); yaz0r e jmimu (motor)",
"publisher":"Infogrames (jogo original); dehalbuq (port independente)",
"genre":"Adventure / Survival Horror",
"image":"./fitdr36s/cover.png",
}
for key,value in values.items(): ET.SubElement(g,key).text=value
ET.indent(rootxml,space="  ")
ET.ElementTree(rootxml).write(pkg/"gameinfo.xml",encoding="utf-8",xml_declaration=True)
splash=game/"splash/credits.bmp"
assert splash.is_file(), "Missing splash artwork"
art=ImageOps.fit(Image.open(splash).convert("RGB"),(640,480),method=Image.Resampling.LANCZOS)
cover=art.copy()
d=ImageDraw.Draw(cover,"RGBA")
d.rectangle((0,360,640,480),fill=(0,0,0,210))
try:
    font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",32)
    small=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",17)
except OSError:
    font=ImageFont.load_default()
    small=font
d.text((18,374),"ALONE IN THE DARK",font=font,fill="white")
d.text((20,432),"FITD R36S  |  RK3326 ARM64  |  dehalbuq",font=small,fill="white")
cover.save(game/"cover.png",optimize=True)
cover.save(pkg/"cover.png",optimize=True)
# Real gameplay screenshot required for official catalog submission;
# credits art is packaged temporarily for personal/manual PortMaster use.
art.save(pkg/"screenshot.png",optimize=True)
(pkg/"README.md").write_text("""# FITD R36S ARM64 - port experimental

Motor independente do Alone in the Dark (1992), compilado para RK3326 AArch64.
Requer arquivos originais legitimamente adquiridos: coloque-os em fitdr36s/data/.
Nao redistribui PAK/ITD comerciais.

Instalacao: extraia o ZIP na pasta /roms/ports/ e execute FITD-R36S-ARM64.sh.
Preserve os saves SAVE*.ITD ao atualizar. Use OpenGL ES 3 e firmware AArch64.
Release sem debug, VSync off, limite de 60 FPS, otimizacao Cortex-A35.
Temporizacao padrao igual a Debug validada; em FPS altos certos eventos podem
acelerar ate que motor e renderizacao sejam completamente desacoplados.
Para diagnostico manual: FITD_R36S_TIMEBASE=real habilita modo experimental
de 25Hz, que anteriormente causou lentidao do personagem.
O screenshot.png e a arte da tela de creditos, nao uma captura real da jogabilidade;
para submissao publica ao PortMaster substitua por screenshot 640x480 do console.

Creditos: jogo original Infogrames; motor yaz0r/jmimu; port dehalbuq;
contribuicoes inspiradas por ReHaunted. Projeto nao oficial.
Repositorio: https://github.com/dehalbuq/FITD-R36S
Consulte fitdr36s/LEIA-ME-LEGAL.txt e LICENSE.FITD.txt.
""")
print("PortMaster: JSON, XML, cover, screenshot and README validated")
