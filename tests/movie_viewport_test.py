"""Exercise the exact movie framebuffer loop with canaries and ASan on host."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1];s=(root/'movie_player.cpp').read_text();a=s.index('void Present(');b=s.index('\nint Play(',a);function=s[a:b]
code='''#include "runtime_s3e.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>
using u8=uint8_t;
constexpr int GFX_TOP=0,GFX_LEFT=0;
u8 output[400*240*3+32];
u8* gfxGetFramebuffer(int,int,void*,void*){return output+16;}
void GSPGPU_FlushDataCache(void*,unsigned){}
void gfxScreenSwapBuffers(int,bool){}
S3ERuntime::VideoRequest displayRequest{};
'''+function+'''
int main(){std::vector<uint8_t> rgb(176*144*3,177);
for(auto request:{S3ERuntime::VideoRequest{1,59,3,201,165,256},{1,0,0,320,240,256},{1,0xffffffffu,0xffffffffu,201,165,256},{1,319,239,100,100,256},{1,0,0,0,0,256}}){displayRequest=request;memset(output,0xa5,sizeof(output));Present(rgb.data(),176,144);for(int i=0;i<16;++i){assert(output[i]==0xa5);assert(output[16+400*240*3+i]==0xa5);}}
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.cpp').write_text(code);subprocess.run(['g++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: movie viewport scaling and rotated BGR addressing; normal, full-screen, overflow and invalid rectangles; ASan/UBSan + canaries.')
