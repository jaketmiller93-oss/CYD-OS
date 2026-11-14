#!/usr/bin/env python3
"""Original 40x32 pixel creature, doubled to CYDOS CIMG RGB565. No external art."""
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
BG=0x11141f
OUTLINE=0x425b66
CREAM=0xf1e5c5
MINT=0xb5e5cf
INK=0x253445
PEACH=0xf0ac9d

def sprite(mood):
    pixels=[[BG]*40 for _ in range(32)]
    shift=-1 if mood=='happy2' else 0
    def dot(x,y,c):
        if 0<=x<40 and 0<=y<32:pixels[y][x]=c
    def rect(x,y,w,h,c):
        for yy in range(y,y+h):
            for xx in range(x,x+w):dot(xx,yy,c)
    def ellipse(cx,cy,rx,ry,c):
        for y in range(cy-ry,cy+ry+1):
            for x in range(cx-rx,cx+rx+1):
                if ((x-cx)*ry)**2+((y-cy)*rx)**2 <= (rx*ry)**2:dot(x,y,c)
    body=0xb9cbd0 if mood=='sick' else MINT
    ellipse(20,30,13,1,0x202b3c)
    rect(10,5+shift,5,12,OUTLINE);rect(11,6+shift,3,11,body)
    rect(26,6+shift,5,11,OUTLINE);rect(27,7+shift,3,10,body)
    rect(12,7+shift,1,5,PEACH);rect(28,8+shift,1,5,PEACH)
    ellipse(20,19+shift,13,11,OUTLINE);ellipse(20,19+shift,12,10,body)
    ellipse(20,23+shift,9,6,CREAM)
    rect(10,28+shift,7,2,OUTLINE);rect(24,28+shift,7,2,OUTLINE)
    rect(11,28+shift,5,1,body);rect(25,28+shift,5,1,body)
    rect(10,21+shift,3,2,PEACH);rect(28,21+shift,3,2,PEACH)
    if mood in ('sleep','blink'):
        rect(14,18+shift,4,1,INK);rect(23,18+shift,4,1,INK)
        dot(14,17+shift,INK);dot(26,17+shift,INK)
    elif mood=='tired':
        rect(14,18,4,1,INK);rect(23,18,4,1,INK)
        rect(15,19,2,2,INK);rect(24,19,2,2,INK)
    else:
        rect(15,16+shift,2,4,INK);rect(24,16+shift,2,4,INK)
        dot(15,16+shift,0xffffff);dot(24,16+shift,0xffffff)
    if mood in ('hungry','tired'):
        rect(19,23+shift,3,3,INK);dot(20,24+shift,PEACH)
    elif mood in ('sick','bored'):
        rect(18,24,5,1,INK)
    else:
        dot(18,23+shift,INK);dot(19,24+shift,INK);dot(20,24+shift,INK)
        dot(21,24+shift,INK);dot(22,23+shift,INK)
    if mood=='dirty':
        rect(10,14,3,2,0x8c795c);rect(27,24,3,3,0x8c795c)
        rect(24,12,2,2,0x8c795c)
    if mood=='sleep':
        for x,y in [(32,5),(35,2)]:
            rect(x,y,3,1,0xc0a5ff);dot(x+1,y+1,0xc0a5ff);rect(x,y+2,3,1,0xc0a5ff)
    if mood=='sick':
        rect(29,9,7,3,0xffc28d);rect(31,7,3,7,0xffc28d)
    if mood in ('happy','happy2'):
        x,y=(5,8) if shift else (35,13)
        rect(x-1,y,3,1,0xffd488);rect(x,y-1,1,3,0xffd488)
    raw=bytearray(b'CIMG'+struct.pack('<HH',80,64))
    for row in pixels:
        expanded=b''.join(struct.pack('<H',((c>>19)&31)<<11|((c>>10)&63)<<5|((c>>3)&31))*2 for c in row)
        raw+=expanded*2
    return raw

def main():
    destination=ROOT/'app/sprites';destination.mkdir(exist_ok=True)
    for mood in ['happy','happy2','blink','hungry','dirty','bored','tired','sleep','sick']:
        (destination/(mood+'.rgb565')).write_bytes(sprite(mood))
if __name__=='__main__':main()
