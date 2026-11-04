# FITD Wii / GX bootstrap v0.1 (original Nintendo Wii)

## Current status

**Hardware diagnostic / 3D graphics prototype ONLY. NOT the playable Alone in the Dark engine.** The original FITD engine has not yet been connected to the GX backend; no original-game files or audio are included.

This bootstrap is the first independently compilable PowerPC Wii target inside the FITD-R36S repository. The Android Test 19 and R36S ARM64 releases are **untouched**.

What is already implemented:
- Native **PowerPC / big-endian** devkitPPC build and Homebrew Channel boot.dol.
- **libogc Hollywood GX** fixed-function 3D, depth-buffered polygon rendering and 480i/480p video selection.
- An original low-polygon animated test scene (not copied from the game).
- **Wii Remote** D-pad/buttons and **GameCube controller** input, SD/FAT mount.
- Discovery of .PAK/.HQR files in private data folder; safe unaligned little-endian decode helpers and self-test.
- SD diagnostic log with GPU test frame average and native endianness.
- Workflow for Wii boot.dol in GitHub Actions; toolchain devkitpro/devkitppc:20261102.

## Install

Extract ZIP to SD card preserving paths:

    SD:/apps/fitd-wii/boot.dol
    SD:/apps/fitd-wii/meta.xml
    SD:/apps/fitd-wii/icon.png
    SD:/apps/fitd-wii/data/

Launch FITD Wii GX Prototype from the Homebrew Channel.

In the diagnostic menu, press Wii Remote A or GameCube A to run GX 3D.
- D-pad: move test character.
- B: rotate.
- Wii Remote HOME / GameCube START: exit.
- A is **not** a Start Game button yet; there is no game engine in this build.

After exiting, check SD:/apps/fitd-wii/wii-diagnostic.txt (or fallback SD:/fitd-wii-diagnostic.txt) for detected archives, framebuffer, GX frame timing, and CPU byte order.

The data folder is intentionally empty. If you want to prepare your legally acquired game files in advance, place them in SD:/apps/fitd-wii/data/. Nothing needs to be copied for this prototype.

## Building

With devkitPro/devkitPPC, libogc and libfat installed:

    cd v2/wii
    make

In Linux Docker (same toolchain as CI):

    docker run --rm -v "$PWD":/src -w /src/v2/wii devkitpro/devkitppc:20261102 make

Output: v2/wii/boot.dol.

## Technical blockers before a playable engine

1. Replace the Linux/Android SDL+EGL/GL backend with Wii **GX**, providing indexed backgrounds and masks, ordered 3D primitives, UI and palette conversion (RGB565/RGB5A3/GX texture tiling).
2. Port original .PAK / HQR / life-script parsing to **big-endian PowerPC**. Old engine casts such as *(s16*)ptr require explicit byte-wise little-endian reads (plus fixes for alignment/struct layout).
3. Integrate native FAT assets, sound (AESND/ASND), controllers, save slots and deterministic scheduling while retaining ARM64/Android Test19 gameplay fixes.
4. Profile memory allocations against Wii MEM1/MEM2 and test on real hardware; 60 FPS is an aspirational benchmark, not promised.

### Legal

Based on Free in the Dark (https://github.com/yaz0r/FITD), with user port improvements in FITD-R36S (https://github.com/dehalbuq/FITD-R36S). FITD engine code is GPL-2.0; any future engine integration must preserve credits and licenses. The prototype contains **no proprietary Alone in the Dark game data**. Obtain game archives from your own legitimately acquired copy. Nintendo is not affiliated with this homebrew project.
