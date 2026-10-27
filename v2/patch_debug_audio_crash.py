#!/usr/bin/env python3
"""Diagnostic-only FITD-R36S audio/room/crash patches.

Runs AFTER v2/patch_upstream.py. Applies to the pinned upstream and fails
closed if the expected signatures changed. Not part of the stable port.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()

def once(path, old, new):
    file = root / path
    text = file.read_text()
    n = text.count(old)
    if n != 1:
        raise RuntimeError(f"debug patch: {path}: expected one match, found {n}: {old[:120]!r}")
    file.write_text(text.replace(old, new, 1))

(root / "FitdLib/r36sCrashDebug.h").write_text(r"""
#pragma once
#ifdef FITD_R36S
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>

inline bool r36sDeepTrace()
{
    const char* flag = std::getenv("FITD_DEEP_TRACE");
    return flag && flag[0] == '1';
}

#define R36S_TRACE(...) do { if (r36sDeepTrace()) { \
    std::fprintf(stderr, "[FITD-DEEP] "); \
    std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); \
    std::fflush(stderr); } } while (0)

inline void r36sCrashSignal(int sig, siginfo_t* info, void*)
{
    char line[192];
    const int n = std::snprintf(line, sizeof(line),
        "\n[FITD-CRASH] signal=%d fault_address=%p pid=%ld\n",
        sig, info ? info->si_addr : nullptr, (long)getpid());
    if (n > 0) write(STDERR_FILENO, line, (size_t)n < sizeof(line) ? n : sizeof(line)-1);
    void* frames[64];
    const int count = backtrace(frames, 64);
    if (count > 0) backtrace_symbols_fd(frames, count, STDERR_FILENO);
    _exit(128 + sig);
}

inline void r36sInstallCrashHandlers()
{
    static bool installed = false;
    if (installed) return;
    installed = true;
    struct sigaction sa = {};
    sa.sa_sigaction = &r36sCrashSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    R36S_TRACE("CRASH HANDLERS ready (SIGSEGV/BUS/ABRT/FPE)");
}
#else
#define R36S_TRACE(...) do {} while (0)
inline void r36sInstallCrashHandlers() {}
#endif
""")

# Lifecycle and camera checkpoints, including safety around a missing
# viewedRoom entry that upstream dereferenced out of bounds.
once("FitdLib/room.cpp",
     '#include "common.h"',
     '#include "common.h"\n#include "r36sCrashDebug.h"')
once("FitdLib/room.cpp",
     '    SaveTimerAnim();\n\n    ASSERT(roomNumber >=0);',
     '''    R36S_TRACE("ROOM ENTER from=%d to=%d floor=%d camera=%d targetActor=%d roomCount=%zu time=%u",
        currentRoom, roomNumber, g_currentFloor, NumCamera,
        currentCameraTargetActor, roomDataTable.size(), (unsigned)timer);
    if (roomNumber < 0 || (size_t)roomNumber >= roomDataTable.size()) {
        std::fprintf(stderr, "[FITD-CRASH] invalid target room=%d; aborting instead of out-of-range access\\n", roomNumber);
        std::abort();
    }
    SaveTimerAnim();

    ASSERT(roomNumber >=0);''')
once("FitdLib/room.cpp",
     '        ASSERT(currentCameraIdx <= g_currentFloorCameraData.size());',
     '''        if (currentCameraIdx >= g_currentFloorCameraData.size()) {
            std::fprintf(stderr, "[FITD-CRASH] bad camera index=%u cameraCount=%zu room=%d slot=%d\\n",
                currentCameraIdx, g_currentFloorCameraData.size(), currentRoom, i);
            std::abort();
        }
        R36S_TRACE("CAMERA room=%d slot=%d absolute=%u viewedCountPending", currentRoom, i, currentCameraIdx);''')
once("FitdLib/room.cpp",
     '''        ASSERT(cameraDataTable[i]->viewedRoomTable[j].viewedRoomIdx == currentRoom);
        if(cameraDataTable[i]->viewedRoomTable[j].viewedRoomIdx == currentRoom) {''',
     '''        if (j >= currentCameraIdx) {
            std::fprintf(stderr, "[FITD-DEEP] CAMERA no matching viewed room: room=%d slot=%d viewedCount=%u\\n",
                currentRoom, i, currentCameraIdx);
            continue; // never dereference viewedRoomTable[viewedCount]
        }
        if(cameraDataTable[i]->viewedRoomTable[j].viewedRoomIdx == currentRoom) {''')
once("FitdLib/room.cpp",
     '''    FlagChangeSalle = 0;
    RestoreTimerAnim();''',
     '''    FlagChangeSalle = 0;
    R36S_TRACE("ROOM READY room=%d cameras=%d newCamera=%d targetActor=%d",
        currentRoom, numCameraInRoom, NewNumCamera, currentCameraTargetActor);
    RestoreTimerAnim();''')

once("FitdLib/mainLoop.cpp",
     '#include "common.h"',
     '#include "common.h"\n#include "r36sCrashDebug.h"')
once("FitdLib/mainLoop.cpp",
     '''void PlayWorld(int allowSystemMenu, int deltaTime)
{
    bool bLoop = true;''',
     '''void PlayWorld(int allowSystemMenu, int deltaTime)
{
    r36sInstallCrashHandlers();
    R36S_TRACE("PLAY WORLD entered, allowMenu=%d dt=%d", allowSystemMenu, deltaTime);
    bool bLoop = true;''')
once("FitdLib/mainLoop.cpp",
     '''        if(FlagChangeSalle)
        {
\t\t\tChangeSalle(NewNumSalle);
            InitView();
            continue;
        }''',
     '''        if(FlagChangeSalle)
        {
            R36S_TRACE("ROOM REQUEST old=%d next=%d actor=%d world=%d time=%u",
                currentRoom, NewNumSalle, currentProcessedActorIdx,
                currentProcessedActorPtr ? currentProcessedActorPtr->indexInWorld : -1,
                (unsigned)timer);
            ChangeSalle(NewNumSalle);
            R36S_TRACE("ROOM SWITCH returned: init view room=%d", currentRoom);
            InitView();
            R36S_TRACE("ROOM VIEW OK room=%d", currentRoom);
            continue;
        }''')
once("FitdLib/mainLoop.cpp",
     '''        GereSwitchCamera();

        // Handle 2d objects''',
     '''        GereSwitchCamera();
#ifdef FITD_R36S
        if (r36sDeepTrace()) {
            static unsigned long r36sGameFrames = 0;
            if ((++r36sGameFrames % 30ul) == 0)
                R36S_TRACE("FRAME n=%lu room=%d player=%d time=%u camera=%d liveActor=%d",
                    r36sGameFrames, currentRoom, currentCameraTargetActor,
                    (unsigned)timer, NumCamera, currentProcessedActorIdx);
        }
#endif
        // Handle 2d objects''')

once("FitdLib/track.cpp",
     '#include "common.h"',
     '#include "common.h"\n#include "r36sCrashDebug.h"')
once("FitdLib/track.cpp",
     '''                        char* link = getRoomLink(currentProcessedActorPtr->room, targetRoomNumber);

                        targetX''',
     '''                        char* link = getRoomLink(currentProcessedActorPtr->room, targetRoomNumber);
                        if (!link) {
                            std::fprintf(stderr, "[FITD-DEEP] NPC no room-link world=%d room=%d targetRoom=%d actor=%d\\n",
                                currentProcessedActorPtr->indexInWorld, currentProcessedActorPtr->room,
                                targetRoomNumber, currentProcessedActorIdx);
                            currentProcessedActorPtr->speed = 0;
                            break;
                        }
                        R36S_TRACE("NPC ROOM LINK actor=%d from=%d to=%d ptr=%p",
                            currentProcessedActorIdx, currentProcessedActorPtr->room,
                            targetRoomNumber, (void*)link);
                        targetX''')

# Original AITD1 sample playback held pointers directly into the HQR cache,
# did not retain/clean up voices, and did not execute queued LM_SAMPLE_THEN.
# In the diagnostic build: bounded owned PCM, validated header, trace every
# start/completion, and play nextSample only after the prior sample ends.
audio = root / "FitdLib/osystemAL.cpp"
source = audio.read_text()
start = source.index("class ITD_AudioSource : public SoLoud::AudioSource")
end = source.index("extern float gVolume;", start)
new = r"""
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "r36sCrashDebug.h"

#ifdef FITD_R36S
extern int r36sSoundEnabled;
#endif
extern int nextSample;
void playSound(int sampleNumber);

class ITD_AudioSource : public SoLoud::AudioSource
{
public:
    std::vector<uint8_t> samples;
    int m_size = 0;
    ITD_AudioSource(const char* samplePtr, int size)
    {
        if (!samplePtr || size < 33) return;
        const uint8_t* hdr = (const uint8_t*)samplePtr + 26;
        const uint32_t packed = uint32_t(hdr[0]) | (uint32_t(hdr[1]) << 8) |
                                (uint32_t(hdr[2]) << 16) | (uint32_t(hdr[3]) << 24);
        const int declared = (int)(packed >> 8) - 2;
        const int available = size - 32;
        if ((packed & 255u) != 1u || declared <= 0 || available <= 0)
            return;
        const int count = std::min(declared, available);
        samples.assign((const uint8_t*)samplePtr + 32,
                       (const uint8_t*)samplePtr + 32 + count);
        m_size = count;
        const unsigned frequencyDiv = (uint8_t)samplePtr[30];
        const unsigned denominator = 256 - frequencyDiv; // always 1..256
        const float rawRate = 1000000.f / denominator;
        // Keep the original rate unless the resource is nonsensical.
        mBaseSamplerate = std::max(4000.f, std::min(rawRate, 48000.f));
        R36S_TRACE("PCM PARSE packed=%u declared=%d available=%d used=%d freqDiv=%u rawRate=%.1f applied=%.1f",
            packed, declared, available, count, frequencyDiv, rawRate, mBaseSamplerate);
        if (declared > available || mBaseSamplerate != rawRate)
            std::fprintf(stderr,
                "[FITD-AUDIO] unusual sample header declared=%d available=%d rawRate=%.1f\\n",
                declared, available, rawRate);
    }
    SoLoud::AudioSourceInstance* createInstance() override;
};

class ITD_AudioInstance : public SoLoud::AudioSourceInstance
{
    ITD_AudioSource* parent;
    unsigned int offset = 0;
public:
    explicit ITD_AudioInstance(ITD_AudioSource* p) : parent(p) {}
    unsigned int getAudio(float* buffer, unsigned int count, unsigned int) override
    {
        for (unsigned int i = 0; i < count; ++i) {
            if (!parent || offset >= parent->samples.size())
                buffer[i] = 0.f;
            else
                buffer[i] = (float(int(parent->samples[offset++]) - 128)) / 128.f;
        }
        return count;
    }
    bool hasEnded() override {
        return !parent || offset >= parent->samples.size();
    }
};

SoLoud::AudioSourceInstance* ITD_AudioSource::createInstance()
{
    return new ITD_AudioInstance(this);
}

struct R36SEffectVoice {
    SoLoud::handle handle = 0;
    std::unique_ptr<ITD_AudioSource> source;
};
static std::vector<R36SEffectVoice> r36sEffectVoices;
static SoLoud::handle r36sLastEffectHandle = 0;

void r36sStopSoundEffects()
{
    if (gSoloud) for (auto& effect : r36sEffectVoices) gSoloud->stop(effect.handle);
    r36sEffectVoices.clear();
    r36sLastEffectHandle = 0;
    nextSample = -1;
    R36S_TRACE("AUDIO STOP samples");
}

void osystem_playSample(char* samplePtr,int size)
{
#ifdef FITD_R36S
    if (!r36sSoundEnabled) return;
#endif
    if (!gSoloud || !samplePtr || size <= 0) {
        std::fprintf(stderr, "[FITD-AUDIO] rejected null/empty sample ptr=%p bytes=%d\\n", (void*)samplePtr, size);
        return;
    }
    if (g_gameId >= TIMEGATE) {
        std::unique_ptr<SoLoud::Wav> wav(new SoLoud::Wav());
        const auto rc = wav->loadMem((const unsigned char*)samplePtr, size, true);
        if (rc) {
            std::fprintf(stderr, "[FITD-AUDIO] WAV load failed rc=%d bytes=%d\\n", (int)rc, size);
            return;
        }
        // TIMEGATE not targeted in this AITD1-specific diagnostic build.
        static std::vector<std::unique_ptr<SoLoud::Wav>> retainedWav;
        gSoloud->play(*wav);
        retainedWav.emplace_back(std::move(wav));
        return;
    }
    std::unique_ptr<ITD_AudioSource> item(new ITD_AudioSource(samplePtr, size));
    if (item->m_size <= 0) {
        std::fprintf(stderr, "[FITD-AUDIO] invalid AITD PCM sample bytes=%d\\n", size);
        return;
    }
    const int decodedCount = item->m_size;
    const float appliedRate = item->mBaseSamplerate;
    const SoLoud::handle voice = gSoloud->play(*item);
    r36sLastEffectHandle = voice;
    r36sEffectVoices.push_back({voice, std::move(item)});
    R36S_TRACE("AUDIO PLAY voice=%u bytes=%d frames=%d sampleRate=%.0f live=%zu queued=%d",
        (unsigned)voice, size, decodedCount, appliedRate,
        r36sEffectVoices.size(), nextSample);
}

"""
source = source[:start] + new + source[end:]
old = """void osystemAL_udpate()
{
    gSoloud->setGlobalVolume(gVolume);
}"""
new_update = r"""void osystemAL_udpate()
{
    if (!gSoloud) return;
    gSoloud->setGlobalVolume(gVolume);
    for (auto it = r36sEffectVoices.begin(); it != r36sEffectVoices.end(); ) {
        if (!gSoloud->isValidVoiceHandle(it->handle)) {
            R36S_TRACE("AUDIO END voice=%u", (unsigned)it->handle);
            it = r36sEffectVoices.erase(it);
        } else {
            ++it;
        }
    }
    if (nextSample >= 0 &&
        (r36sLastEffectHandle == 0 ||
         !gSoloud->isValidVoiceHandle(r36sLastEffectHandle))) {
        const int queued = nextSample;
        nextSample = -1;
        R36S_TRACE("AUDIO SEQUENCE: completed first sample; play next=%d", queued);
        playSound(queued & 0x3fff);
    }
}"""
if source.count(old) != 1:
    raise RuntimeError("debug patch: osystemAL_udpate signature changed")
audio.write_text(source.replace(old, new_update, 1))

once("FitdLib/life.cpp", '#include "common.h"',
     '#include "common.h"\n#include "r36sCrashDebug.h"\nextern void r36sStopSoundEffects();')
once("FitdLib/life.cpp",
     '''            case LM_STOP_SAMPLE: // todo
            {
                appendFormated("LM_STOP_SAMPLE ");
                //printf("LM_STOP_SAMPLE\\n");''',
     '''            case LM_STOP_SAMPLE: // todo
            {
                appendFormated("LM_STOP_SAMPLE ");
                R36S_TRACE("LIFE STOP SAMPLE actor=%d room=%d", currentProcessedActorIdx, currentRoom);
                r36sStopSoundEffects();
                //printf("LM_STOP_SAMPLE\\n");''')
once("FitdLib/life.cpp",
     '''            case LM_SAMPLE_THEN: //todo
            {
                appendFormated("LM_SAMPLE_THEN ");''',
     '''            case LM_SAMPLE_THEN: //todo
            {
                appendFormated("LM_SAMPLE_THEN ");
                R36S_TRACE("LIFE SAMPLE_THEN actor=%d room=%d timer=%u", currentProcessedActorIdx, currentRoom, (unsigned)timer);''')
once("FitdLib/tatou.cpp",
     '''    char* ptr = HQR_Get(listSamp,num);
    assert(ptr);

    osystem_playSample(ptr, size);''',
     '''    char* ptr = HQR_Get(listSamp,num);
    if (!ptr || size <= 32) {
        fprintf(stderr, "[FITD-AUDIO] failed loading sample number=%d bytes=%d\\n", num, size);
        return;
    }
#ifdef FITD_R36S
    const char* deep = getenv("FITD_DEEP_TRACE");
    if (deep && deep[0] == '1')
        fprintf(stderr, "[FITD-DEEP] SOUND sample=%d size=%d next=%d room=%d timer=%u\\n",
                num, size, nextSample, currentRoom, (unsigned)timer);
#endif
    osystem_playSample(ptr, size);''')

print("FITD-R36S diagnostic audio ownership/queue + room/camera/track/crash tracing installed")


# 2026-10-27 first diagnostic field test: immediate SIGSEGV before the
# gameplay worker starts. The video thread entered osystemAL_udpate before
# upstream game init set nextSample=-1; nextSample's zero-initialized global
# was mistaken for an armed script sequence, so playSound(0) dereferenced
# an unavailable LISTSAMP. Also install the crash handler BEFORE this point.
once("FitdLib/osystemAL.cpp",
     '#include "osystemAL.h"',
     '''#include "osystemAL.h"
#include "r36sCrashDebug.h"
extern int nextSample;''')
once("FitdLib/osystemAL.cpp",
     '''void osystemAL_init()
{
    gSoloud = new SoLoud::Soloud();''',
     '''void osystemAL_init()
{
    r36sInstallCrashHandlers(); // before SDL event loop / worker startup
    nextSample = -1;            // global begins as zero before game initialization
    R36S_TRACE("AUDIO INIT: nextSample reset to -1");
    gSoloud = new SoLoud::Soloud();''')
once("FitdLib/osystemAL.cpp",
     '#include <memory>',
     '''#include <memory>
#include <mutex>
#include <atomic>''')
once("FitdLib/osystemAL.cpp",
     '''static SoLoud::handle r36sLastEffectHandle = 0;

void r36sStopSoundEffects()''',
     '''static SoLoud::handle r36sLastEffectHandle = 0;
// SoLoud update runs on SDL thread and samples play on the game worker;
// keep the owner array synchronized and arm queues only from life scripts.
static std::mutex r36sAudioMutex;
static std::atomic<int> r36sQueuedScriptSample{-1};

void r36sQueueNextSample(int sample)
{
    nextSample = sample;
    r36sQueuedScriptSample.store(sample, std::memory_order_release);
    R36S_TRACE("AUDIO QUEUE armed sample=%d after voice=%u",
        sample, (unsigned)r36sLastEffectHandle);
}

void r36sAudioPollQueued()
{
    const int pending = r36sQueuedScriptSample.load(std::memory_order_acquire);
    if (pending < 0 || !gSoloud) return;
    bool completed = false;
    {
        std::lock_guard<std::mutex> guard(r36sAudioMutex);
        completed = r36sLastEffectHandle != 0 &&
                    !gSoloud->isValidVoiceHandle(r36sLastEffectHandle);
    }
    if (!completed) return;
    const int sample = r36sQueuedScriptSample.exchange(-1, std::memory_order_acq_rel);
    if (sample < 0) return;
    nextSample = -1;
    R36S_TRACE("AUDIO SEQUENCE worker: previous effect ended, next=%d", sample);
    playSound(sample & 0x3fff); // HQR accesses stay on the game worker
}

void r36sStopSoundEffects()''')
once("FitdLib/osystemAL.cpp",
     '''    if (gSoloud) for (auto& effect : r36sEffectVoices) gSoloud->stop(effect.handle);
    r36sEffectVoices.clear();
    r36sLastEffectHandle = 0;
    nextSample = -1;''',
     '''    std::lock_guard<std::mutex> guard(r36sAudioMutex);
    if (gSoloud) for (auto& effect : r36sEffectVoices) gSoloud->stop(effect.handle);
    r36sEffectVoices.clear();
    r36sLastEffectHandle = 0;
    nextSample = -1;
    r36sQueuedScriptSample.store(-1, std::memory_order_release);''')
once("FitdLib/osystemAL.cpp",
     '''    const SoLoud::handle voice = gSoloud->play(*item);
    r36sLastEffectHandle = voice;
    r36sEffectVoices.push_back({voice, std::move(item)});
    R36S_TRACE("AUDIO PLAY voice=%u bytes=%d frames=%d sampleRate=%.0f live=%zu queued=%d",
        (unsigned)voice, size, decodedCount, appliedRate,
        r36sEffectVoices.size(), nextSample);''',
     '''    SoLoud::handle voice = 0;
    size_t liveCount = 0;
    {
        std::lock_guard<std::mutex> guard(r36sAudioMutex);
        voice = gSoloud->play(*item);
        r36sLastEffectHandle = voice;
        r36sEffectVoices.push_back({voice, std::move(item)});
        liveCount = r36sEffectVoices.size();
    }
    R36S_TRACE("AUDIO PLAY voice=%u bytes=%d frames=%d sampleRate=%.0f live=%zu queued=%d",
        (unsigned)voice, size, decodedCount, appliedRate,
        liveCount, nextSample);''')
once("FitdLib/osystemAL.cpp",
     '''    for (auto it = r36sEffectVoices.begin(); it != r36sEffectVoices.end(); ) {
        if (!gSoloud->isValidVoiceHandle(it->handle)) {
            R36S_TRACE("AUDIO END voice=%u", (unsigned)it->handle);
            it = r36sEffectVoices.erase(it);
        } else {
            ++it;
        }
    }
    if (nextSample >= 0 &&
        (r36sLastEffectHandle == 0 ||
         !gSoloud->isValidVoiceHandle(r36sLastEffectHandle))) {
        const int queued = nextSample;
        nextSample = -1;
        R36S_TRACE("AUDIO SEQUENCE: completed first sample; play next=%d", queued);
        playSound(queued & 0x3fff);
    }''',
     '''    // SDL event thread: housekeeping only; HQR/playSound must stay on
    // the game worker, after its own initialization has completed.
    std::lock_guard<std::mutex> guard(r36sAudioMutex);
    for (auto it = r36sEffectVoices.begin(); it != r36sEffectVoices.end(); ) {
        if (!gSoloud->isValidVoiceHandle(it->handle)) {
            R36S_TRACE("AUDIO END voice=%u", (unsigned)it->handle);
            it = r36sEffectVoices.erase(it);
        } else {
            ++it;
        }
    }''')

once("FitdLib/life.cpp",
     '''                    playSound(evalVar());
                    nextSample = evalVar();''',
     '''                    playSound(evalVar());
                    r36sQueueNextSample(evalVar());''')
once("FitdLib/life.cpp",
     'extern void r36sStopSoundEffects();',
     '''extern void r36sStopSoundEffects();
extern void r36sQueueNextSample(int);''')
once("FitdLib/tatou.cpp",
     '#include "common.h"',
     '''#include "common.h"
#ifdef FITD_R36S
extern void r36sAudioPollQueued();
#endif''')
once("FitdLib/tatou.cpp",
     '''\tu32 timeIncrease = osystem_startOfFrame();
    assert(timeIncrease);''',
     '''\tu32 timeIncrease = osystem_startOfFrame();
#ifdef FITD_R36S
    r36sAudioPollQueued(); // invoked only after game worker is active
#endif
    assert(timeIncrease);''')
print("R36S audio startup guard, worker-only queued samples and synchronized SoLoud owners installed")


# Field test 2026-10-27: the saved game aborted inside makeSaveFile,
# std::vector<tWorldObject>::operator[]; AITD1 forces 300 world-object
# records on disk while LoadWorld initializes only maxObjects (<300).
# The correction retains the original 300-record save file format and
# hardens BOTH serialization directions.
file = root / "FitdLib/save.cpp"
src = file.read_text()
begin_load = src.index("int loadSave(int saveNumber)")
begin_save = src.index("int makeSaveFile(int entry)", begin_load)
load_section, save_section = src[begin_load:begin_save], src[begin_save:]
old = '''        oldNumMaxObj = maxObjects;
        maxObjects = 300; // fix for save engine..'''
new = '''        oldNumMaxObj = maxObjects;
        maxObjects = 300; // original AITD1 on-disk format always uses 300
        if (ListWorldObjets.size() < 300) {
            const size_t originalSize = ListWorldObjets.size();
            ListWorldObjets.resize(300);
            for (size_t j = originalSize; j < 300; ++j)
                ListWorldObjets[j].objIndex = -1;
            fprintf(stderr, "[FITD-SAVE] LOAD padded world objects from %zu to 300\\n",
                    originalSize);
        }'''
if load_section.count(old) != 1 or save_section.count(old) != 1:
    raise RuntimeError("save/load 300 entries signature changed")
load_section=load_section.replace(old,new,1)
save_section=save_section.replace(old,new.replace("LOAD padded", "SAVE padded"),1)

# Never destroy the previous working SAVE*.ITD if an abort or I/O error
# happens during a new save. Serialize to .tmp and atomically rename.
old='''    sprintf(buffer,"SAVE%d.ITD",entry);

    fHandle = fopen(buffer,"wb+");'''
new='''    sprintf(buffer,"SAVE%d.ITD",entry);
    char temporary[128];
    snprintf(temporary, sizeof(temporary), "SAVE%d.ITD.tmp", entry);
    R36S_TRACE("SAVE BEGIN slot=%d worldSize=%zu maxObjects=%d floor=%d room=%d",
               entry, ListWorldObjets.size(), maxObjects,
               g_currentFloor, currentRoom);
    fHandle = fopen(temporary,"wb+");'''
if save_section.count(old)!=1:
    raise RuntimeError("save file opener changed")
save_section=save_section.replace(old,new,1)

old='''    fclose(fHandle);

    return 1;
}

int makeSave(int arg0)'''
new='''    const long savedBytes = ftell(fHandle);
    const int error = ferror(fHandle);
    const int closeError = fclose(fHandle);
    if (error || closeError || savedBytes < 0) {
        fprintf(stderr, "[FITD-SAVE] write failed slot=%d bytes=%ld err=%d close=%d\\n",
                entry, savedBytes, error, closeError);
        remove(temporary);
        return 0;
    }
    if (rename(temporary, buffer) != 0) {
        perror("[FITD-SAVE] rename failed; old save preserved");
        remove(temporary);
        return 0;
    }
    fprintf(stderr, "[FITD-SAVE] SAVE COMPLETE slot=%d bytes=%ld actualWorld=%zu\\n",
            entry, savedBytes, ListWorldObjets.size());
    return 1;
}

int makeSave(int arg0)'''
if save_section.count(old)!=1:
    raise RuntimeError("save file close/return changed")
save_section=save_section.replace(old,new,1)
file.write_text(src[:begin_load]+load_section+save_section)
print("R36S save/load world-object 300-entry safety + atomic save replacement installed")

# The door sequence plays lengthy samples 3 (1.9 s) and 4 (2.7 s).
# During an automatic-door trigger, life scripts sometimes call playSound
# every frame (at 60 FPS), causing >100 voice overlaps and 'stretched'/
# stuttering door effects. Restrict only these two door samples to at
# most once per ~1.5 s; do not change footsteps, music or other effects.
file = root / "FitdLib/tatou.cpp"
src = file.read_text()
anchor = '''void playSound(int num)
{
\tif(num == -1)
\t\treturn;'''
if src.count(anchor) != 1:
    raise RuntimeError("door sample debounce anchor changed")
replacement = '''void playSound(int num)
{
\tif(num == -1)
\t\treturn;
#ifdef FITD_R36S
    if (num == 3 || num == 4) {
        static unsigned lastTick[2] = {0, 0};
        static bool played[2] = {false, false};
        static unsigned suppressed[2] = {0, 0};
        const int slot = num - 3;
        // timer advances every gameplay frame on the legacy engine.
        if (played[slot] && (unsigned)(timer - lastTick[slot]) < 90u) {
            ++suppressed[slot];
            if (suppressed[slot] == 1 || suppressed[slot] % 60 == 0)
                fprintf(stderr,
                    "[FITD-AUDIO] DOOR SAMPLE SUPPRESSED sample=%d count=%u room=%d time=%u\\\\n",
                    num, suppressed[slot], currentRoom, (unsigned)timer);
            return;
        }
        lastTick[slot] = (unsigned)timer;
        played[slot] = true;
        suppressed[slot] = 0;
    }
#endif'''
src=src.replace(anchor,replacement,1)
file.write_text(src)
print("R36S repeated door sample (3,4) overlap limiter installed for debug")
