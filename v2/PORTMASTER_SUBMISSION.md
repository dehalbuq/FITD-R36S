# PortMaster submission checklist: FITD R36S

Official references:
- https://portmaster.games/porting.html
- https://portmaster.games/packaging.html
- https://portmaster.games/build-environments.html

## Current scope
- Engine: open-source FITD (requires user-owned Alone in the Dark 1 data).
- Binary: AArch64, SDL3/KMSDRM, OpenGL ES 3.0.
- Build baseline: Ubuntu 22.04 arm64 (glibc <=2.35 audit).
- PortMaster control script integration: control.txt, mod_CFW_NAME, get_controls,
  GPTOKEYB, pm_platform_helper, pm_finish, libs.aarch64.
- Debug and crash backtraces: disabled in release.

## Submission blockers
1. Provide an actual 4:3 gameplay screenshot with minimum 640x480 pixels.
   Save as `v2/port/assets/screenshot.png`. Title art or splash is not valid.
   A cover is optional; if supplied it must also show gameplay.
2. Validate on multiple supported firmwares and device families, recording
   boot, SDL3/KMSDRM and EGL/GLES3, buttons, sound, save/load, quit, FPS,
   and no missing GLIBC/GLIBCXX symbols. Do not claim universal support.
3. Verify required runtime symbols for all bundled libraries. Never bundle
   the firmware's glibc, dynamic loader, EGL, GBM, DRM or Mali GPU drivers.
4. Respect licenses for FITD, source dependencies, bundled libraries,
   generated splash images and any cover image.
5. Check port name uniqueness in PortMaster-New and put files under
   `ports/fitdr36s/` in the submission fork. The downloadable ZIP contains
   launcher and the `fitdr36s/` game directory at its root.

## Smoke-test matrix (unverified until evidence is recorded)
| Firmware | Device | Boot | GLES3 | Input | Sound | Save | Exit |
| --- | --- | --- | --- | --- | --- | --- | --- |
| dArkOSRE | R36S RK3326 | previously tested debug | pending release | pending | pending | pending | pending |
| ArkOS | RK3326 AArch64 | pending | pending | pending | pending | pending | pending |
| AmberELEC | supported ARM64 | pending | pending | pending | pending | pending | pending |
| ROCKNIX | supported ARM64 | pending | pending | pending | pending | pending | pending |
| muOS/Knulli | supported ARM64 | pending | pending | pending | pending | pending | pending |

## Build
Push to `release/r36s-aarch64-safe-20261101` to produce `fitdr36s.zip`.
For a submission-gated build, set `PORTMASTER_SUBMISSION=1`.
This causes packaging to FAIL without the real gameplay screenshot.

## Frame rate
The current debug-compatible per-frame timing avoids the known slow
movement regression, but can accelerate some events at high FPS.
A future timing refactor must separate render frames from simulation updates.

## Firmware test procedure for a real R36S

1. Back up `fitdr36s/data/SAVE*.ITD` before updating.
2. Install the latest `fitdr36s.zip`, copying only your own original data.
3. On the handheld's terminal/SSH, run:
   ```sh
   cd /roms/ports/fitdr36s
   sh ./check_compatibility.sh
   ```
   If your firmware mounts ROMs elsewhere, change the base path.
4. Send `fitdr36s/compatibility-report.txt` after each firmware test.
5. Test New Game, action mode, entering/exiting rooms, inventory,
   save/load, sound, Start+Select exit, and character walking pace.
6. Record actual FPS if your firmware provides a supported overlay.
   Do not equate 60 render FPS with correct simulation timing.
7. Capture one real 4:3 gameplay screenshot (>=640x480), place it at
   `v2/port/assets/screenshot.png` for public PortMaster submission.
8. Only mark a firmware compatible after gameplay, controls, sound,
   save/load and exit pass on the real device.

## Important remaining constraints

- ARMHF/32-bit firmware is **not** supported by the current ARM64 package.
- OpenGL ES 2.0 is **not** supported by the current GLES3 engine.
- SDL3/KMSDRM behaviour is firmware-specific and may require integration
  with PortMaster's WestonPack runtime in a separate version.
- Ubuntu 22.04 reduces the required GLIBC baseline but does not
  automatically guarantee compatibility with every PortMaster firmware.
- The current Release build must pass GitHub Actions and physical gameplay
  tests before its compatibility can be claimed.

## ArkOS4Clone Kinhank K36: verified diagnostic (7 Nov 2025 device clock)

- Linux AArch64 / kernel 4.4.189 / Ubuntu 19.10 / glibc **2.30**.
- The deployed executable requires GLIBC_2.34 and bundled GNU libraries
  require GLIBC_2.35. Loader rejection occurs BEFORE SDL/EGL starts.
- The firmware has /dev/dri/card0, renderD128, EGL/GLES/GBM Mali links and
  SDL2; these are not sufficient to prove SDL3/GLES3 initialization works.
- Original game .PAK data is present; do not copy or redistribute game files.
- PortMaster control.txt exists under /opt/system/Tools/PortMaster.
- The user's previously tested dArkOSRE binary **MUST NOT be overwritten**
  by the experimental legacy build.

## Backward compatibility strategy

- Modern dArkOSRE: keep the original fitd.aarch64 and libs.aarch64/.
- Older glibc <= 2.30: use fitd.glibc230.aarch64 and libs.glibc230/
  ONLY after each old binary/library passes the readelf ABI audit.
- Launcher chooses using getconf GNU_LIBC_VERSION and reports a clear error
  if the old binary has not been packaged; it does not run a known-bad ABI.
- Do not change port.json's min_glibc from 2.35 until a verified **single ZIP
  containing both** binaries and both lib sets is built and tested.
- After the ABI issue is fixed, capture actual game startup logs, then check
  Mali GLES3/EGL/KMSDRM, audio, input mapping, save/load and quit separately
  on BOTH ArkOS4Clone and dArkOSRE.
- No firmware compatibility is guaranteed by a successful compile alone.
