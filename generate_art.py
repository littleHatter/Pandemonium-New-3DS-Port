from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parent
lines=['#pragma once','#include <cstdint>']
def array(name,data):
 lines.append('alignas(4) inline constexpr uint8_t '+name+'[]={')
 lines.extend(','.join(str(x) for x in data[n:n+32])+',' for n in range(0,len(data),32));lines.append('};')
im=Image.open(r/'artwork/pandemonium3ds_bottomscreen.png').convert('RGBA')
assert im.size==(320,240)
im=Image.alpha_composite(Image.new('RGBA',im.size,(0,0,0,255)),im).convert('RGB');b=bytearray(320*240*2)
for y in range(240):
 for x in range(320):
  red,green,blue=im.getpixel((x,y));i=(x*240+239-y)*2;pixel=((red>>3)<<11)|((green>>2)<<5)|(blue>>3);b[i:i+2]=pixel.to_bytes(2,"little")
array('kBottomArt',b)
for name,file,size in [('A','A.png',(83,17)),('B','B.png',(77,17)),('B2','B2.png',(77,17)),('Start','START.png',(160,17)),('Loading','pandemonium_loading.png',(38,8))]:
 im=Image.open(r/'artwork'/file).convert('RGBA');assert im.size==size;array('k'+name+'Art',im.tobytes())
(r/'bottom_art.h').write_text('\n'.join(lines)+'\n')
# Deterministic launcher-size conversion; retain the supplied full-size original.
im=Image.open(r/'artwork/pandemonium_icon.png').convert('RGBA');im=Image.alpha_composite(Image.new('RGBA',im.size,(0,0,0,255)),im).convert('RGB');im.resize((48,48),Image.Resampling.LANCZOS).save(r/'icon.png')
