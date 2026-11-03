// Android 4.4 ARMv7: smoke-test ACTUAL upstream FITD core code.
#include "config.h"
#include "unpack.h"
#include "pak.h"
#include "cosTable.h"
#include <SDL.h>
#include <cstring>
#include <cstdio>
#include <cstdint>

extern "C" {
    // The original FITD PAK routines append PAK names to this directory.
    // Keep this as an Android app-private path and never ship original assets.
    char homePath[512] = {};
}

extern "C" int fitd_android_core_selftest() {
    // The original engine's fixed-point cosine lookup table must be linked.
    if (cosTable[0] != 4) {
        SDL_Log("FITD-CORE: cosine table verification failed: %d", cosTable[0]);
        return 10;
    }
    const char* privatePath = SDL_AndroidGetInternalStoragePath();
    if (!privatePath) {
        SDL_Log("FITD-CORE: Android private storage unavailable");
        return 11;
    }
    if (std::strlen(privatePath) + 3 >= sizeof(homePath)) return 12;
    std::snprintf(homePath, sizeof(homePath), "%s/", privatePath);
    // FITD's real PAK directory reader should safely find no game data
    // in an app that deliberately ships without commercial PAK assets.
    if (PAK_getNumFiles("FITD-CORE-NONEXISTENT") != 0) {
        SDL_Log("FITD-CORE: unexpected PAK on clean install");
        return 13;
    }
    // Known independent raw-deflate fixture, not copyrighted game data.
    // Original FITD PAK_deflate uses zlib raw DEFLATE (-15).
    const unsigned char compressed[] = {
        0x73,0xf3,0x0c,0x71,0xd1,0x0d,0x32,0x36,0x0b,0xd6,
        0x75,0xf4,0x73,0x09,0xf2,0xf7,0x74,0xd1,0x75,0x0c,
        0xf2,0x0d,0x33,0x07,0x00
    };
    static const char expected[] = "FITD-R36S-ANDROID-ARMV7";
    unsigned char decompressed[sizeof(expected)] = {};
    unsigned char scratch[sizeof(compressed)];
    std::memcpy(scratch, compressed, sizeof(scratch));
    const int rc = PAK_deflate(scratch, decompressed,
                               sizeof(compressed), sizeof(expected)-1);
    if (rc != 0 || std::memcmp(decompressed, expected, sizeof(expected)-1) != 0) {
        SDL_Log("FITD-CORE: original PAK decompression failed, rc=%d", rc);
        return 14;
    }
    SDL_Log("FITD-CORE: PASS - original FITD PAK file access, raw-DEFLATE and cosTable "
            "code runs on ARMv7 API19 (no commercial assets)");
    return 0;
}
