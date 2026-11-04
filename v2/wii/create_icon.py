#!/usr/bin/env python3
"""Generate a compact 128x48 RGB Homebrew Channel icon without font assets."""
import struct
import sys
import zlib
from pathlib import Path

W,H=128,48
canvas=[[(9,16,30) for _ in range(W)] for _ in range(H)]
def rect(x,y,w,h,color):
    for yy in range(max(y,0),min(y+h,H)):
        for xx in range(max(x,0),min(x+w,W)):
            canvas[yy][xx]=color
rect(0,39,128,9,(25,31,45))
rect(4,3,120,2,(72,133,171))
rect(4,42,120,2,(72,133,171))
# Small low-poly "door" silhouette alongside FITD
rect(9,10,28,28,(40,75,103))
rect(12,13,22,25,(12,23,42))
rect(15,16,16,22,(18,37,58))
rect(29,28,2,2,(157,186,147))
# 5x7 pixel glyphs
GLYPHS={
"F":["11111","10000","11110","10000","10000","10000","10000"],
"I":["11111","00100","00100","00100","00100","00100","11111"],
"T":["11111","00100","00100","00100","00100","00100","00100"],
"D":["11110","10001","10001","10001","10001","10001","11110"],
"W":["10001","10001","10001","10101","10101","11011","10001"],
}
def draw(word,x,y,scale):
    for letter in word:
        for yy,line in enumerate(GLYPHS[letter]):
            for xx,v in enumerate(line):
                if v=="1":rect(x+xx*scale,y+yy*scale,scale,scale,(198,225,225))
        x+=6*scale
draw("FITD",45,11,3)
draw("WII",85,32,1)
def chunk(tag,payload):
    return (struct.pack(">I",len(payload))+tag+payload+
            struct.pack(">I",zlib.crc32(tag+payload)&0xffffffff))
data=bytearray()
for y in range(H):
    data.append(0)
    for x in range(W):
        data.extend(canvas[y][x])
png=(b"\x89PNG\r\n\x1a\n"+
     chunk(b"IHDR",struct.pack(">IIBBBBB",W,H,8,2,0,0,0))+
     chunk(b"IDAT",zlib.compress(bytes(data),9))+
     chunk(b"IEND",b""))
path=Path(sys.argv[1]) if len(sys.argv)>1 else Path("icon.png")
path.parent.mkdir(parents=True,exist_ok=True)
path.write_bytes(png)
print("Created",path,"PNG",W,"x",H)
