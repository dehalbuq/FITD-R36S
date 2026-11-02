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
