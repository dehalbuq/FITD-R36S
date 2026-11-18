#!/usr/bin/env python3
"""Repair pinned SoLoud SDL3 stream-format bug on Lakka ARMHF.

The upstream SDL3 backend mixed using the physical device format despite
SDL_OpenAudioDeviceStream explicitly requesting a separate source format.
SDL converts between formats itself; SoLoud must mix in the stream's source
format. Keep edits limited to that backend so other platforms are unaffected.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_lakka_sdl3_backend.py SOURCE_DIR")
path = Path(sys.argv[1]) / "ThirdParty/soloud.cmake/soloud/src/backend/sdl3_static/soloud_sdl3_static.cpp"
s = path.read_text()
if "[LAKKA-SDL3-AUDIO]" in s:
    print("[LAKKA-SDL3-AUDIO] backend already patched")
    raise SystemExit(0)

start_token = "        gBackendData.audioStream = SDL_OpenAudioDeviceStream(\n"
end_token = "\n        aSoloud->postinit_internal("
if s.count(start_token) != 2 or s.count(end_token) != 1:
    raise SystemExit("Unexpected SoLoud SDL3 backend signature; refusing unsafe patch")
start = s.index(start_token, s.index("        SDL_AudioSpec as{};"))
end = s.index(end_token, start)

replacement = r"""        // SDL3 stream format can differ from physical HDMI ALSA format.
        // SoLoud's mixer must use the stream *source* format.
        gBackendData.audioStream = SDL_OpenAudioDeviceStream(
            deviceId, &as, soloud_sdl3static_audiomixer, static_cast<void*>(aSoloud));

        if (!gBackendData.audioStream)
        {
            SDL_Log("[LAKKA-SDL3-AUDIO] F32 open failed: %s", SDL_GetError());
            as.format = SDL_AUDIO_S16;
            gBackendData.audioStream = SDL_OpenAudioDeviceStream(
                deviceId, &as, soloud_sdl3static_audiomixer, static_cast<void*>(aSoloud));
        }

        if (!gBackendData.audioStream)
        {
            SDL_Log("[LAKKA-SDL3-AUDIO] S16 open failed: %s", SDL_GetError());
            return UNKNOWN_ERROR;
        }

        gBackendData.audioDeviceId = SDL_GetAudioStreamDevice(gBackendData.audioStream);
        if (!gBackendData.audioDeviceId ||
            !SDL_GetAudioStreamFormat(gBackendData.audioStream, &gBackendData.activeAudioSpec, NULL))
        {
            SDL_Log("[LAKKA-SDL3-AUDIO] cannot query stream source format: %s", SDL_GetError());
            SDL_DestroyAudioStream(gBackendData.audioStream);
            gBackendData.audioStream = NULL;
            return UNKNOWN_ERROR;
        }
        SDL_Log("[LAKKA-SDL3-AUDIO] HDMI stream source format=0x%x freq=%d channels=%u device=%u",
            (unsigned)gBackendData.activeAudioSpec.format, gBackendData.activeAudioSpec.freq,
            (unsigned)gBackendData.activeAudioSpec.channels, (unsigned)gBackendData.audioDeviceId);
"""
s = s[:start] + replacement + s[end:]

old = "        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(gBackendData.audioStream));"
new = """        if (!SDL_ResumeAudioDevice(gBackendData.audioDeviceId))
        {
            SDL_Log("[LAKKA-SDL3-AUDIO] resume failed: %s", SDL_GetError());
            return UNKNOWN_ERROR;
        }
        SDL_Log("[LAKKA-SDL3-AUDIO] playback resumed");"""
if s.count(old) != 1:
    raise SystemExit("Expected SDL3 resume anchor not found")
s = s.replace(old, new, 1)
path.write_text(s)
print("[LAKKA-SDL3-AUDIO] corrected source format and robust device startup")
