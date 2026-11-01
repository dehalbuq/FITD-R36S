#!/usr/bin/env python3
"""Generate two native 640x480 startup BMP pages for FITD-R36S.

The public GitHub profile picture is downloaded at BUILD TIME only.
If it is not reachable, render an attractive @dehalbuq monogram instead.
Nothing is downloaded when the user launches the game or while offline.
Original AITD assets are NEVER read or embedded in either startup image.
"""
from __future__ import annotations
from io import BytesIO
from pathlib import Path
from urllib.request import Request, urlopen
import os
import sys

from PIL import Image, ImageDraw, ImageFont, ImageOps

if len(sys.argv) != 2:
    raise SystemExit("usage: generate_startup_splash.py OUTPUT_DIR")
output = Path(sys.argv[1])
output.mkdir(parents=True, exist_ok=True)

SIZE=(640,480)
DARK=(10,13,23)
WHITE=(239,237,226)
MUTED=(178,185,195)
GOLD=(221,172,100)
BLUE=(104,167,200)
RED=(236,145,130)
FONT_DIR=Path("/usr/share/fonts/truetype/dejavu")
regular=FONT_DIR/"DejaVuSansCondensed.ttf"
bold=FONT_DIR/"DejaVuSansCondensed-Bold.ttf"
if not bold.is_file():
    bold=FONT_DIR/"DejaVuSans-Bold.ttf"

def f(size: int, strong=False):
    return ImageFont.truetype(str(bold if strong else regular), size)

def base():
    img=Image.new("RGB",SIZE,DARK)
    p=img.load()
    for y in range(SIZE[1]):
        for x in range(SIZE[0]):
            wave=int(3*(x/SIZE[0]))+int(7*(y/SIZE[1]))
            p[x,y]=(DARK[0]+wave,DARK[1]+wave,DARK[2]+wave)
    d=ImageDraw.Draw(img)
    for x in range(16,640,48):
        d.line((x,0,x,480),fill=(20,25,34),width=1)
    for y in range(28,480,48):
        d.line((0,y,640,y),fill=(20,25,34),width=1)
    d.rounded_rectangle((18,18,622,462),radius=17,
                        fill=(15,19,28),outline=(72,62,48),width=2)
    return img,d

def centered(d,s,y,font,fill):
    bb=d.textbbox((0,0),s,font=font)
    x=(SIZE[0]-(bb[2]-bb[0]))//2
    d.text((x,y),s,font=font,fill=fill)
    return x

def line(d,s,x,y,font,fill):
    d.text((x,y),s,font=font,fill=fill)

def fitline(d,s,font,x,maxw):
    if d.textlength(s,font=font) <= maxw:
        return [s]
    words=s.split()
    rows=[];cur=""
    for word in words:
        test=(cur+" "+word).strip()
        if d.textlength(test,font=font)>maxw and cur:
            rows.append(cur);cur=word
        else:cur=test
    if cur:rows.append(cur)
    return rows

def wrapped(d,s,x,y,font,fill,maxw,lineheight=22):
    for part in fitline(d,s,font,x,maxw):
        line(d,part,x,y,font,fill);y+=lineheight
    return y

def avatar():
    url="https://avatars.githubusercontent.com/u/209284539?v=4"
    try:
        req=Request(url,headers={"User-Agent":"FITD-R36S-CI-Splash/1.0"})
        with urlopen(req,timeout=12) as response:
            binary=response.read(2_000_001)
        if len(binary)>2_000_000:
            raise ValueError("profile picture too large")
        pic=Image.open(BytesIO(binary))
        pic.load()
        pic=ImageOps.fit(pic.convert("RGB"),(116,116),centering=(0.5,0.5))
        print("[FITD-SPLASH] GitHub profile avatar successfully downloaded")
        return pic
    except Exception as exc:
        print("[FITD-SPLASH] avatar unavailable; offline monogram fallback:",str(exc)[:100])
        pic=Image.new("RGB",(116,116),(38,56,72))
        pd=ImageDraw.Draw(pic)
        ft=f(74,True)
        box=pd.textbbox((0,0),"d",font=ft)
        pd.text(((116-(box[2]-box[0]))//2,11),"d",font=ft,fill=GOLD)
        return pic

def place_avatar(canvas,x,y,photo):
    circle=Image.new("L",(116,116),0)
    cd=ImageDraw.Draw(circle)
    cd.ellipse((0,0,115,115),fill=255)
    canvas.paste(photo,(x,y),circle)
    d=ImageDraw.Draw(canvas)
    d.ellipse((x-2,y-2,x+117,y+117),outline=GOLD,width=3)

# PAGE ONE -- concise source attribution, not claiming port author created FITD.
page,d=base()
centered(d,"FITD-R36S",32,f(43,True),WHITE)
centered(d,"FREE IN THE DARK  /  PORT ARM64",89,f(15),GOLD)
d.line((40,116,600,116),fill=(91,75,55),width=1)
place_avatar(page,52,132,avatar())
d=ImageDraw.Draw(page)
line(d,"PORT PARA R36S",199,131,f(13,True),MUTED)
line(d,"@dehalbuq",195,153,f(34,True),WHITE)
line(d,"github.com/dehalbuq/FITD-R36S",196,198,f(15),BLUE)
d.line((38,265,602,265),fill=(91,75,55),width=1)
line(d,"CRÉDITOS E ORIGENS DO CÓDIGO",47,277,f(17,True),GOLD)
line(d,"Motor FITD: Vincent Hamm (yaz0r) e colaboradores",47,310,f(16),WHITE)
line(d,"Histórico do código: jmimu/FITD",47,337,f(16),WHITE)
line(d,"Referências de correções: ReHaunted / spacefarergames",47,364,f(15),WHITE)
line(d,"Adaptação R36S: @dehalbuq e colaboradores",47,390,f(15),WHITE)
centered(d,"Projeto independente. Não oficial.",432,f(13),MUTED)
page.save(output/"credits.bmp",format="BMP")

# PAGE TWO -- legally meaningful explanation, short enough to read at 640x480.
page,d=base()
centered(d,"AVISO LEGAL",29,f(31,True),GOLD)
centered(d,"MOTOR ABERTO  •  JOGO COMERCIAL",73,f(15,True),MUTED)
d.line((42,109,598,109),fill=(91,75,55),width=1)
centered(d,"ESTE PACOTE NÃO INCLUI O JOGO.",122,f(18,True),RED)
items=[
    ("Inclui o motor aberto FITD, patches e scripts do port R36S.",WHITE),
    ("Não inclui PAK/ITD do jogo, cenários, músicas, modelos,",WHITE),
    ("vozes ou outros recursos comerciais de Alone in the Dark.",WHITE),
    ("Para jogar, obtenha uma cópia legítima do jogo original",WHITE),
    ("e forneça os arquivos necessários em fitdr36s/data/.",WHITE),
    ("Marcas, personagens e conteúdos pertencem a seus titulares.",MUTED),
    ("Este port não é oficial nem afiliado aos donos do jogo.",MUTED),
    ("Código do motor: GPL v2 ou posterior, conforme seus avisos.",MUTED),
    ("Respeite as licenças do motor e os direitos sobre os dados.",MUTED),
]
y=170
for item,color in items:
    centered(d,item,y,f(15),color)
    y+=26
d.line((42,414,598,414),fill=(91,75,55),width=1)
centered(d,"Leia LEIA-ME-LEGAL.txt e LICENSE.FITD.txt",426,f(14,True),GOLD)
centered(d,"Pressione A, START ou uma tecla para continuar",448,f(12),MUTED)
page.save(output/"legal.bmp",format="BMP")
for file in (output/"credits.bmp",output/"legal.bmp"):
    if Image.open(file).size != SIZE:
        raise SystemExit(f"bad splash dimensions: {file}")
    print("[FITD-SPLASH] wrote",file, file.stat().st_size,"bytes")
