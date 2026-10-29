#!/usr/bin/env python3
"""R36S safety for evaluated world-object indexes (ReHaunted comparison).

ReHaunted already bounds-checks the explicit evalVar objectNumber. This patch
also guards a few indirect operands used by found-life scripts (the chest).
Never changes valid-object semantics; logs invalid references for follow-up.
"""
from pathlib import Path
import sys
if len(sys.argv) != 2:
    raise SystemExit("usage: patch_cellar_evalvar.py UPSTREAM_SOURCE_DIR")
p=Path(sys.argv[1]) / "FitdLib/evalVar.cpp"
s=p.read_text()
if "[FITD-CELLAR] EVAL" in s:
    raise RuntimeError("already patched evalVar")
def sub(tag,old,new,n):
    global s
    found=s.count(old)
    if found != n:
        raise RuntimeError(f"{tag}: expected {n}, got {found}")
    s=s.replace(old,new)
sub("explicit world operand",
'''                actorIdx = ListWorldObjets[objectNumber].objIndex;

                currentLifePtr+=2;''',
'''                currentLifePtr+=2;
#ifdef FITD_R36S
                if (objectNumber < 0 ||
                    static_cast<size_t>(objectNumber) >= ListWorldObjets.size()) {
                    fprintf(stderr,
                        "[FITD-CELLAR] EVAL_BAD_OBJECT world=%d size=%zu floor=%d room=%d time=%u\n",
                        objectNumber, ListWorldObjets.size(), g_currentFloor, currentRoom,
                        (unsigned)timer);
                    fflush(stderr);
                    return 0;
                }
#endif
                actorIdx = ListWorldObjets[objectNumber].objIndex;''',2)
sub("recursive found var",
'''                        if(ListWorldObjets[evalVar()].foundFlag & 0x8000)''',
'''                        const int foundIdx = evalVar();
#ifdef FITD_R36S
                        if (foundIdx < 0 ||
                            static_cast<size_t>(foundIdx) >= ListWorldObjets.size()) {
                            fprintf(stderr,
                                "[FITD-CELLAR] EVAL_BAD_FOUND world=%d size=%zu room=%d time=%u\n",
                                foundIdx, ListWorldObjets.size(), currentRoom, (unsigned)timer);
                            fflush(stderr);
                            return 0;
                        }
#endif
                        if(ListWorldObjets[foundIdx].foundFlag & 0x8000)''',2)
sub("first dist world operand",
'''                        int actorNumber = ListWorldObjets[*(s16*)currentLifePtr].objIndex;
                        currentLifePtr+=2;''',
'''                        int objectNumber = *(s16*)currentLifePtr;
                        currentLifePtr+=2;
#ifdef FITD_R36S
                        if (objectNumber < 0 ||
                            static_cast<size_t>(objectNumber) >= ListWorldObjets.size()) {
                            fprintf(stderr,
                                "[FITD-CELLAR] EVAL_BAD_DIST world=%d size=%zu room=%d\n",
                                objectNumber, ListWorldObjets.size(), currentRoom);
                            return 32000;
                        }
#endif
                        int actorNumber = ListWorldObjets[objectNumber].objIndex;''',1)
sub("second dist world operand",
'''                        int objectIdx = ListWorldObjets[worldObjectIdx].objIndex;''',
'''#ifdef FITD_R36S
                        if (worldObjectIdx < 0 ||
                            static_cast<size_t>(worldObjectIdx) >= ListWorldObjets.size()) {
                            fprintf(stderr,
                                "[FITD-CELLAR] EVAL_BAD_DIST world=%d size=%zu room=%d\n",
                                worldObjectIdx, ListWorldObjets.size(), currentRoom);
                            return 32000;
                        }
#endif
                        int objectIdx = ListWorldObjets[worldObjectIdx].objIndex;''',1)
sub("relative position world operand",
'''                        if(ListWorldObjets[objNum].objIndex == -1)''',
'''#ifdef FITD_R36S
                        if (objNum < 0 || static_cast<size_t>(objNum) >= ListWorldObjets.size()) {
                            fprintf(stderr,
                                "[FITD-CELLAR] EVAL_BAD_REL world=%d size=%zu room=%d\n",
                                objNum, ListWorldObjets.size(), currentRoom);
                            return 0;
                        }
#endif
                        if(ListWorldObjets[objNum].objIndex == -1)''',2)
p.write_text(s)
print("FITD-R36S: ReHaunted-style world-object index validation in evalVar installed")
