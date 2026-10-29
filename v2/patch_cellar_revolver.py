#!/usr/bin/env python3
"""Isolated crash-safe inventory life scripts + ReHaunted object render flag fix.

Upstream: yaz0r/FITD @ 543e22bc.
Compare: spacefarergames/AloneInTheDarkReHaunted:
  TatouSource/FitdLib/main.cpp GenereAffList (AF_SPECIAL | AF_ANIMATED).
Preserves original behavior for valid indexes and existing HIT_OBJ/audio/save.
"""
import sys
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit("usage: patch_cellar_revolver.py UPSTREAM_SOURCE_DIR")
root = Path(sys.argv[1])

def edit(rel, edits):
    path = root / rel
    contents = path.read_text()
    if "[FITD-CELLAR]" in contents:
        raise RuntimeError(f"already patched {rel}")
    for label, old, new, count in edits:
        found = contents.count(old)
        if found != count:
            raise RuntimeError(f"{rel}: {label}: expected {count} anchors, found {found}")
        contents = contents.replace(old, new)
    path.write_text(contents)
    print(f"patched {rel}")

edit("FitdLib/main.cpp", [
 ("guard found-life world index",
  '''\tif(objIdx == -1)
\t\treturn;

\tfoundLife = ListWorldObjets[objIdx].foundLife;''',
  '''#ifdef FITD_R36S
    if (objIdx < 0 || static_cast<size_t>(objIdx) >= ListWorldObjets.size()) {
        if (objIdx != -1)
            fprintf(stderr, "[FITD-CELLAR] BAD_FOUND_INDEX idx=%d size=%zu floor=%d room=%d time=%u\\n",
                    objIdx, ListWorldObjets.size(), g_currentFloor, currentRoom, (unsigned)timer);
        return;
    }
    if (g_gameId == AITD1 && g_currentFloor == 3 && currentRoom == 10) {
        fprintf(stderr,
            "[FITD-CELLAR] FOUND_ACTION idx=%d life=%d objActor=%d flags=%d body=%d name=%d action=%d time=%u\\n",
            objIdx, (int)ListWorldObjets[objIdx].foundLife,
            (int)ListWorldObjets[objIdx].objIndex,
            (int)ListWorldObjets[objIdx].foundFlag, (int)ListWorldObjets[objIdx].foundBody,
            (int)ListWorldObjets[objIdx].foundName, action, (unsigned)timer);
        fflush(stderr);
    }
#else
\tif(objIdx == -1)
\t\treturn;
#endif

\tfoundLife = ListWorldObjets[objIdx].foundLife;''',1),
 ("correct object animation flag exactly as ReHaunted",
  '''if(!(actorPtr->objectType & (AF_SPECIAL & AF_ANIMATED)))''',
  '''if(!(actorPtr->objectType & (AF_SPECIAL | AF_ANIMATED)))''',1),
 ("trace stage objects at cellar entry",
  '''\tfor(int i=0;i<maxObjects;i++)
\t{
        tWorldObject* currentObject = &ListWorldObjets[i];''',
  '''#ifdef FITD_R36S
    static int lastCellarFloor = -1, lastCellarRoom = -1;
    const bool r36sDumpCellar = (g_gameId == AITD1 && g_currentFloor == 3 &&
        currentRoom == 10 && (lastCellarRoom != 10 || lastCellarFloor != 3));
    lastCellarFloor = g_currentFloor;
    lastCellarRoom = currentRoom;
    if (r36sDumpCellar)
        fprintf(stderr, "[FITD-CELLAR] WORLD_SCAN_BEGIN floor=%d room=%d objects=%d\\n",
                g_currentFloor, currentRoom, maxObjects);
#endif
\tfor(int i=0;i<maxObjects;i++)
\t{
        tWorldObject* currentObject = &ListWorldObjets[i];
#ifdef FITD_R36S
        if (r36sDumpCellar && currentObject->stage == g_currentFloor &&
            (currentObject->room == currentRoom || currentObject->lifeMode == 0)) {
            fprintf(stderr,
                "[FITD-CELLAR] WORLD idx=%d room=%d body=%d anim=%d life=%d mode=%d flags=%d foundLife=%d actor=%d\\n",
                i, currentObject->room, currentObject->body, currentObject->anim,
                currentObject->life, currentObject->lifeMode, currentObject->flags,
                currentObject->foundLife, currentObject->objIndex);
        }
#endif''',1),
 ("trace cellar actor rendering",
  '''\t\t\t\tNbAffObjets++;
\t\t\t}
\t\t}
\t}
}

void InitView()''',
  '''#ifdef FITD_R36S
                if (g_gameId == AITD1 && g_currentFloor == 3 && currentRoom == 10) {
                    static unsigned cellarDrawChecks = 0;
                    ++cellarDrawChecks;
                    if (cellarDrawChecks < 60 || cellarDrawChecks % 900 == 0) {
                        fprintf(stderr,
                            "[FITD-CELLAR] DRAW actor=%d world=%d body=%d anim=%d type=%d camera=%d time=%u\\n",
                            i, actorPtr->indexInWorld, actorPtr->bodyNum,
                            actorPtr->ANIM, actorPtr->objectType, NumCamera, (unsigned)timer);
                    }
                }
#endif
\t\t\t\tNbAffObjets++;
\t\t\t}
\t\t}
\t}
}

void InitView()''',1)
])
edit("FitdLib/life.cpp", [
 ("guard explicit life target",
  '''            var_6 = *(s16*)(currentLifePtr);
            currentLifePtr += 2;

            if (var_6 == -1)''',
  '''            var_6 = *(s16*)(currentLifePtr);
            currentLifePtr += 2;
#ifdef FITD_R36S
            if (var_6 < 0 || static_cast<size_t>(var_6) >= ListWorldObjets.size()) {
                fprintf(stderr,
                    "[FITD-CELLAR] INVALID_TARGET life=%d target=%d opcode=0x%04x size=%zu caller=%d room=%d floor=%d time=%u\\n",
                    lifeNum, var_6, (unsigned)(currentOpcode & 0xffff),
                    ListWorldObjets.size(), currentProcessedActorIdx,
                    currentRoom, g_currentFloor, (unsigned)timer);
                fflush(stderr);
                // Cannot interpret an unknown target object; stop this script,
                // rather than continuing with corrupted state or modifying saves.
                exitLife = 1;
                break;
            }
#endif

            if (var_6 == -1)''',1),
 ("trace found life opcode for crash pinpointing",
  '''        appendFormated("%d:opcode: %02X: ", lifeNum, currentOpcode & 0xFFFF);

        if (currentOpcode & 0x8000)''',
  '''        appendFormated("%d:opcode: %02X: ", lifeNum, currentOpcode & 0xFFFF);
#ifdef FITD_R36S
        if (callFoundLife && g_gameId == AITD1 && g_currentFloor == 3 && currentRoom == 10) {
            fprintf(stderr,
                "[FITD-CELLAR] SCRIPT life=%d opcode=0x%04x sourceActor=%d sourceWorld=%d frame=%d time=%u\\n",
                lifeNum, (unsigned)(currentOpcode & 0xffff), currentProcessedActorIdx,
                currentProcessedActorPtr ? currentProcessedActorPtr->indexInWorld : -1,
                currentProcessedActorPtr ? currentProcessedActorPtr->frame : -1,
                (unsigned)timer);
            fflush(stderr);
        }
#endif

        if (currentOpcode & 0x8000)''',1),
 ("guard delete target without changing valid cases",
  '''                deleteObject(lifeTempVar1);

                if (ListWorldObjets[lifeTempVar1].foundBody != -1)''',
  '''#ifdef FITD_R36S
                if (lifeTempVar1 < 0 ||
                    static_cast<size_t>(lifeTempVar1) >= ListWorldObjets.size()) {
                    fprintf(stderr,
                        "[FITD-CELLAR] INVALID_DELETE life=%d obj=%d size=%zu found=%d floor=%d room=%d time=%u\\n",
                        lifeNum, lifeTempVar1, ListWorldObjets.size(),
                        callFoundLife ? 1 : 0, g_currentFloor, currentRoom, (unsigned)timer);
                    fflush(stderr);
                    break;
                }
#endif
                deleteObject(lifeTempVar1);

                if (ListWorldObjets[lifeTempVar1].foundBody != -1)''',1),
 ("guard copy angle target",
  '''                int object = readNextArgument("object");
                int localObjectIndex = ListWorldObjets[object].objIndex;''',
  '''                int object = readNextArgument("object");
#ifdef FITD_R36S
                if (object < 0 || static_cast<size_t>(object) >= ListWorldObjets.size()) {
                    fprintf(stderr,
                        "[FITD-CELLAR] INVALID_COPY_ANGLE life=%d obj=%d size=%zu room=%d time=%u\\n",
                        lifeNum, object, ListWorldObjets.size(), currentRoom, (unsigned)timer);
                    fflush(stderr);
                    break;
                }
#endif
                int localObjectIndex = ListWorldObjets[object].objIndex;''',1),
 ("guard camera target",
  '''#endif

                if (lifeTempVar1 != currentWorldTarget) // same target''',
  '''#endif
#ifdef FITD_R36S
                if (lifeTempVar1 < 0 ||
                    static_cast<size_t>(lifeTempVar1) >= ListWorldObjets.size()) {
                    fprintf(stderr,
                        "[FITD-CELLAR] INVALID_CAMERA_TARGET life=%d obj=%d size=%zu room=%d time=%u\n",
                        lifeNum, lifeTempVar1, ListWorldObjets.size(), currentRoom, (unsigned)timer);
                    fflush(stderr);
                    break;
                }
#endif

                if (lifeTempVar1 != currentWorldTarget) // same target''',1)
])
print("FITD-R36S: cellar bounds checking, found-life tracing and ReHaunted render flag fix installed")
