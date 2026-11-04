# Wii native FITD engine adaptation plan

Target: original Nintendo Wii / PowerPC Broadway / ATI Hollywood GX fixed-function.
Do not change or destabilize Android Test19 or stable R36S ARM64 branches.

| Source component | Wii action | Priority |
|---|---|---|
| Android Test19 gameplay patches | Apply on pinned FITD source with endian-safe archive readers | High |
| FitdLib C++ engine | Cross-compile with powerpc-eabi-g++, disable Linux SDL/platform dependencies | High |
| osystem_* graphics abstraction | Implement GX backend with native packed textures, 3D polygons, overlays, masks | High |
| Archive/parser pointer casts | Read explicit LE16/LE32 and avoid misaligned access | Blocker |
| osystem_* sound | Replace Android SDL2 OPL+PCM output with Wii AESND + mixer | Medium |
| Gamepad | Map Wii Remote/Nunchuk + Classic/GC to FITD actions, not to emulator | Medium |
| Timing | Keep time-dependent legacy scripts correct while delivering smooth poses | High |
| Build | devkitPPC + libogc + libfat + tests in GitHub Actions | Done for bootstrap |
| Licensed data | Copy only from user's owned game to SD at install time | No redistribution |

Milestones:
- M0 (this commit): POWERPC boot.dol, GX depth-buffered scene, SD scanner, controllers, build CI.
- M1: Confirm boot on real Wii (please share log); fix GX/video/controller issues.
- M2: Load and decode room background and masks from original assets with strict LE parsing.
- M3: Geometry primitive renderer backed by GX; run actual FITD scene data.
- M4: Full scripts/AI, audio and saves; apply gameplay patches after careful endian review.
- M5: Stability checks and packaging as fully playable open-source engine.

Do not claim playable AITD until M4 succeeds on hardware.
