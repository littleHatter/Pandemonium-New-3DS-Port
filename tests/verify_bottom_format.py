"""Check generated RGB565 pixels and exercise the exact native overlay loop on host."""
from pathlib import Path
from PIL import Image
import re,struct,subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
header=(root/'bottom_art.h').read_text()
data=bytes(map(int,re.search(r'kBottomArt\[\]=\{(.*?)\};',header,re.S).group(1).replace('\n','').rstrip(',').split(',')))
assert len(data)==320*240*2
im=Image.open(root/'artwork/pandemonium3ds_bottomscreen.png').convert('RGBA');im=Image.alpha_composite(Image.new('RGBA',im.size,(0,0,0,255)),im).convert('RGB')
for y in range(240):
 for x in range(320):
  r,g,b=im.getpixel((x,y));expected=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
  assert struct.unpack_from('<H',data,2*(x*240+239-y))[0]==expected
main=(root/'main_3ds.cpp').read_text();start=main.index('    auto blit=');end=main.index('\n    };',start)+len('\n    };');loop=main[start:end]
code='''#include "bottom_art.h"
#include <cstdint>
#include <cstring>
#include <cassert>
int main(){
uint16_t guarded[320*240+2]{};auto* screen=guarded+1;
'''+loop+'''
for(unsigned state=0;state<16;++state){
 guarded[0]=guarded[320*240+1]=0xa55a;
 memcpy(screen,kBottomArt,sizeof(kBottomArt));
 if(state&1)blit(kAArt,83,17,0,0);
 if(state&2)blit(kStartArt,160,17,83,0);
 if(state&8)blit(kB2Art,77,17,243,0);else if(state&4)blit(kBArt,77,17,243,0);
 blit(kLoadingArt,38,8,282,232);
 assert(guarded[0]==0xa55a&&guarded[320*240+1]==0xa55a);
}
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: 76800 RGB565 artwork pixels match; 16 overlay states + loading pass bounds guards and ASan/UBSan.')
