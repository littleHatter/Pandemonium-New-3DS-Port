#pragma once
#include "cpu.h"
#include "loader.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
// The original localized softkey renderer is Thumb at 0x4A013E8C.
// Input r0 is a localized string pointer; r1 selects its screen edge.
class PandemoniumUi {
    Cpu* cpu;uint32_t pending=0;
    uint32_t word(uint32_t at){uint32_t v=0;cpu->MemRead(at,&v,4);return v;}
    static void DrawHook(Cpu* c,uint64_t,uint32_t,void* user){
        auto& self=*static_cast<PandemoniumUi*>(user);
        uint32_t text=0,lr=0,flags=0;c->RegRead(0,&text);c->RegRead(Cpu::REG_LR,&lr);c->RegRead(Cpu::REG_CPSR,&flags);
        uint32_t prompt=self.Classify(text);self.pending|=prompt;
        if(getenv("PAN_UI_TRACE")){static unsigned last=~0u;if(last!=prompt){fprintf(stderr,"[Softkey] pointer=%08x prompt=%u caller=%08x\n",text,prompt,lr);last=prompt;}}
        // Suppress only this dedicated drawing routine. No glyphs are erased
        // from the framebuffer and the game's input/menu handling is untouched.
        flags=(flags&~32u)|((lr&1)?32u:0u);c->RegWrite(Cpu::REG_CPSR,&flags);c->RegWrite(Cpu::REG_PC,&lr);
    }
public:
    enum : uint32_t {A=1,Start=2,B=4,Retry=8};
    explicit PandemoniumUi(Cpu* c):cpu(c){}
    uint32_t Classify(uint32_t text){
        if(!text)return 0;
        struct Mapping{uint32_t id,button;};
        static constexpr Mapping ids[]={{8,A},{15,A},{20,A},{21,A},{26,B},{30,A},{44,Retry},{110,Retry},{165,Retry},{174,Retry},{202,B},{238,Retry},{241,A},{248,A},{267,B},{296,Retry},{303,A},{305,A},{311,Start}};
        const uint32_t base=word(G(0x4a0a5d64)),table=word(G(0x4a0a5d68));
        if(base&&table)for(auto m:ids)if(base+word(table+4*m.id)==text)return m.button;
        // Some callers supply a raw string instead of a localization-table entry.
        char label[24]{};for(unsigned n=0;n<sizeof(label)-1;++n){if(cpu->MemRead(text+n,label+n,1)||!label[n])break;if(label[n]>='a'&&label[n]<='z')label[n]-='a'-'A';}
        if(!strcmp(label,"SKIP")||!strcmp(label,"SELECT")||!strcmp(label,"NEXT"))return A;
        if(!strcmp(label,"PAUSE"))return Start;if(!strcmp(label,"BACK"))return B;if(!strcmp(label,"RETRY"))return Retry;return 0;
    }
    bool Install(){
        static const uint8_t expected[]={0xf3,0xb5,0x83,0xb0,0x03,0x9a,0x00,0x2a};uint8_t actual[8]{};
        if(cpu->MemRead(G(0x4a013e8c),actual,8)||memcmp(actual,expected,8))return false;
        return cpu->AddCodeHook(DrawHook,this,G(0x4a013e8c),G(0x4a013e8c))==Cpu::OK;
    }
    uint32_t Take(){const auto value=pending;pending=0;return value;}
};
