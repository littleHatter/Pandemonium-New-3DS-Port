#include "pandemonium_ui.h"
#include "audio_gain.h"
#include <map>
#include <cassert>
#include <climits>
uint32_t ImageBase(){return STOCK_BASE;}
class FakeCpu:public Cpu{
public:
 std::map<uint32_t,uint8_t> mem;uint32_t regs[17]{};CodeHook fn=nullptr;void* context=nullptr;
 void put(uint32_t a,uint32_t v){for(int i=0;i<4;++i)mem[a+i]=uint8_t(v>>(8*i));}
 bool IsNative()const override{return false;}
 int MemMap(uint64_t,size_t)override{return 0;}
 int MemRead(uint64_t a,void* out,size_t n)override{auto p=(uint8_t*)out;for(size_t i=0;i<n;++i){auto it=mem.find(a+i);if(it==mem.end())return ERR_READ_UNMAPPED;p[i]=it->second;}return 0;}
 int MemWrite(uint64_t a,const void* data,size_t n)override{auto p=(const uint8_t*)data;for(size_t i=0;i<n;++i)mem[a+i]=p[i];return 0;}
 int RegRead(int r,void* out)override{memcpy(out,regs+r,4);return 0;}
 int RegWrite(int r,const void* v)override{memcpy(regs+r,v,4);return 0;}
 int AddCodeHook(CodeHook f,void* u,uint64_t a,uint64_t b)override{assert(a==0x4a013e8c&&b==a);fn=f;context=u;return 0;}
 int AddSvcHook(SvcHook,void*)override{return 0;}
 int AddWriteHook(WriteHook,void*,uint64_t,uint64_t)override{return 0;}
 int Start(uint64_t,uint64_t,size_t)override{return 0;}
 int Stop()override{return 0;}
 const char* ErrorText(int)const override{return "test";}
};
int main(){
 FakeCpu c;const uint8_t prologue[]={0xf3,0xb5,0x83,0xb0,0x03,0x9a,0x00,0x2a};c.MemWrite(0x4a013e8c,prologue,8);
 c.put(0x4a0a5d64,0x60000000);c.put(0x4a0a5d68,0x61000000);
 PandemoniumUi ui(&c);assert(ui.Install());
 const std::pair<uint32_t,uint32_t> ids[]={{8,1},{15,1},{20,1},{21,1},{26,4},{30,1},{44,8},{110,8},{165,8},{174,8},{202,4},{238,8},{241,1},{248,1},{267,4},{296,8},{303,1},{305,1},{311,2}};
 for(auto pair:ids){uint32_t offset=100+pair.first*32;c.put(0x61000000+4*pair.first,offset);const char otherLanguage[]="UNE AUTRE LANGUE";c.MemWrite(0x60000000+offset,otherLanguage,sizeof(otherLanguage));assert(ui.Classify(0x60000000+offset)==pair.second);}
 c.regs[0]=0x60000000+100+21*32;c.regs[14]=0x4a001001;c.regs[16]=0x10;c.fn(&c,0x4a013e8c,2,c.context);assert(c.regs[15]==0x4a001001&&(c.regs[16]&32));
 c.regs[0]=0x60000000+100+26*32;c.fn(&c,0x4a013e8c,2,c.context);assert(ui.Take()==5);assert(ui.Take()==0);
 c.mem[0x4a013e8c]=0;PandemoniumUi mismatch(&c);assert(!mismatch.Install());
 for(int sample=-32768;sample<=32767;++sample){int expected=sample*2;if(expected>32767)expected=32767;if(expected<-32768)expected=-32768;assert(PandemoniumDoubleVolume(sample)==expected);}
 puts("PASS: 19 localized IDs, prompt combination/reset, return state, signature guard, all 65536 audio values");
}
