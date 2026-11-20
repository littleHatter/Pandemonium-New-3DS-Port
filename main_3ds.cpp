#include <3ds.h>
#include <sys/iosupport.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <exception>
#include <new>
#include "loader.h"
#include "runtime_s3e.h"
#include "runtime_clock.h"
#include "audio_gain.h"
#include "bottom_art.h"
#include "movie_player.h"

u32 __stacksize__=1024*1024;
u32 __ctru_heap_size=24*1024*1024;
u32 __ctru_linear_heap_size=4*1024*1024;
extern "C" {
extern char *fake_heap_start,*fake_heap_end;
extern u32 __ctru_heap,__ctru_linear_heap;
void __system_allocateHeaps(){
    if(R_FAILED(svcControlMemory(&__ctru_heap,OS_HEAP_AREA_BEGIN,0,__ctru_heap_size,MEMOP_ALLOC,(MemPerm)(MEMPERM_READ|MEMPERM_WRITE))))svcBreak(USERBREAK_PANIC);
    if(R_FAILED(svcControlMemory(&__ctru_linear_heap,0,0,__ctru_linear_heap_size,MEMOP_ALLOC_LINEAR,(MemPerm)(MEMPERM_READ|MEMPERM_WRITE))))svcBreak(USERBREAK_PANIC);
    mappableInit(OS_MAP_AREA_BEGIN,OS_MAP_AREA_END);fake_heap_start=(char*)__ctru_heap;fake_heap_end=fake_heap_start+__ctru_heap_size;
}
}
namespace {
constexpr const char* ROOT="sdmc:/3ds/pandemonium";
PrintConsole console;
const devoptab_t* originalOutput=nullptr;
const devoptab_t* originalError=nullptr;
const devoptab_t* consoleOutput=nullptr;
devoptab_t quietOutput{},faultOutput{};
bool faultConsole=false;
ssize_t QuietWrite(struct _reent*,void*,const char*,size_t length){
    return length;
}
void ActivateFaultConsole();
ssize_t FaultWrite(struct _reent* r,void* fd,const char* text,size_t length){
    if(faultConsole&&consoleOutput&&consoleOutput->write_r)consoleOutput->write_r(r,fd,text,length);
    if(faultConsole){gfxFlushBuffers();gfxScreenSwapBuffers(GFX_BOTTOM,false);}
    return length;
}
void ActivateFaultConsole(){
    if(!faultConsole){
        faultConsole=true;
        devoptab_list[STD_OUT]=&quietOutput;devoptab_list[STD_ERR]=&faultOutput;
    }
    consoleSelect(&console);
}
uint32_t bottomPrompts=0;bool firstGameFrame=false;
void DrawBottomArt(){
    if(faultConsole)return;
    u16* screen=reinterpret_cast<u16*>(gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,nullptr,nullptr));memcpy(screen,kBottomArt,sizeof(kBottomArt));
    auto blit=[&](const uint8_t* rgba,unsigned w,unsigned h,unsigned left,unsigned top){
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
            const auto* p=rgba+4*(y*w+x);auto& out=screen[(left+x)*240+239-top-y];const unsigned alpha=p[3];
            const unsigned red=(((out>>11)&31)*255/31*(255-alpha)+p[0]*alpha+127)/255;
            const unsigned green=(((out>>5)&63)*255/63*(255-alpha)+p[1]*alpha+127)/255;
            const unsigned blue=((out&31)*255/31*(255-alpha)+p[2]*alpha+127)/255;
            out=((red>>3)<<11)|((green>>2)<<5)|(blue>>3);
        }
    };
    if(bottomPrompts&1)blit(kAArt,83,17,0,0);
    if(bottomPrompts&2)blit(kStartArt,160,17,83,0);
    if(bottomPrompts&8)blit(kB2Art,77,17,243,0);else if(bottomPrompts&4)blit(kBArt,77,17,243,0);
    if(!firstGameFrame)blit(kLoadingArt,38,8,282,232);
    static_assert(sizeof(kBottomArt)==320*240*2,"Bottom artwork must match RGB565 framebuffer");
    GSPGPU_FlushDataCache(screen,sizeof(kBottomArt));
}
aptHookCookie appHookCookie;
void OnAppEvent(APT_HookType hook,void*){if(hook==APTHOOK_ONRESTORE)DrawBottomArt();}
void WaitAfterError(){
    std::fprintf(stderr,"\nPress A to return, or use HOME.\n");
    while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_A)break;gspWaitForVBlank();}
}

class Audio3DS {
    static constexpr unsigned COUNT=12,SAMPLES=512;
    ndspWaveBuf buffers[COUNT]{};s16* pcm=nullptr;unsigned next=0,queued=0,dropped=0;bool ready=false,playing=false;
public:
    bool Init(){
        next=queued=dropped=0;playing=false;memset(buffers,0,sizeof(buffers));
        Result rc=ndspInit();if(R_FAILED(rc)){printf("[Audio3DS] NDSP init failed: %08lx; continuing without output\n",(unsigned long)rc);return false;}
        pcm=(s16*)linearAlloc(COUNT*SAMPLES*2);
        if(!pcm){puts("[Audio3DS] PCM allocation failed");ndspExit();return false;}
        ready=true;ndspChnReset(0);ndspChnSetInterp(0,NDSP_INTERP_LINEAR);ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM16);ndspChnSetRate(0,22050.0f);
        float mix[12]{};mix[0]=mix[1]=1.0f;ndspChnSetMix(0,mix);ndspChnSetPaused(0,true);
        for(unsigned i=0;i<COUNT;++i)buffers[i].data_pcm16=pcm+i*SAMPLES;
        puts("[Audio3DS] NDSP ready: 22050 Hz mono");return true;
    }
    void Push(const int16_t* samples,size_t count,uint32_t rate){
        if(!ready)return;
        if(count>SAMPLES||rate!=22050){if(!dropped++)puts("[Audio3DS] Unexpected PCM format");return;}
        auto& b=buffers[next];
        if(b.status==NDSP_WBUF_QUEUED||b.status==NDSP_WBUF_PLAYING){if(++dropped==1||dropped%100==0)printf("[Audio3DS] Queue full; dropped blocks=%u\n",dropped);return;}
        for(size_t i=0;i<count;++i)b.data_pcm16[i]=PandemoniumDoubleVolume(samples[i]);DSP_FlushDataCache(b.data_pcm16,count*2);b.nsamples=count;b.offset=0;b.looping=false;b.status=NDSP_WBUF_FREE;b.next=nullptr;
        ndspChnWaveBufAdd(0,&b);next=(next+1)%COUNT;++queued;
        if(!playing&&queued>=2){ndspChnSetPaused(0,false);playing=true;}
    }
    void Shutdown(){if(ready){ndspChnWaveBufClear(0);ndspChnReset(0);ndspExit();linearFree(pcm);pcm=nullptr;ready=false;}playing=false;}
    ~Audio3DS(){Shutdown();}

};
}
extern "C" void ShowLogOnScreen(){ActivateFaultConsole();}

int main(){
    gfxInitDefault();gfxSet3D(false);gfxSetScreenFormat(GFX_TOP,GSP_BGR8_OES);
    gfxSetScreenFormat(GFX_BOTTOM,GSP_RGB565_OES);gfxSetDoubleBuffering(GFX_BOTTOM,false);
    originalOutput=devoptab_list[STD_OUT];originalError=devoptab_list[STD_ERR];
    consoleInit(GFX_BOTTOM,&console);consoleDebugInit(debugDevice_CONSOLE);
    consoleOutput=devoptab_list[STD_ERR];
    quietOutput=*originalOutput;quietOutput.name="pan_log";quietOutput.write_r=QuietWrite;
    faultOutput=*originalError;faultOutput.name="pan_fault";faultOutput.write_r=FaultWrite;
    devoptab_list[STD_OUT]=&quietOutput;devoptab_list[STD_ERR]=&faultOutput;
    setvbuf(stdout,nullptr,_IONBF,0);setvbuf(stderr,nullptr,_IONBF,0);
    aptSetHomeAllowed(true);DrawBottomArt();gfxFlushBuffers();gspWaitForVBlank();aptHook(&appHookCookie,OnAppEvent,nullptr);
    std::error_code ec;std::filesystem::create_directories(ROOT,ec);
    int result=1;bool isNew=false;APT_CheckNew3DS(&isNew);
    try {
    if(!isNew)std::fprintf(stderr,"This application requires a New Nintendo 3DS / New 2DS XL.\n");
    else{
        osSetSpeedupEnable(true);uint64_t before=NfsClockMs();svcSleepThread(20000000);uint64_t elapsed=NfsClockMs()-before;
        printf("[3DS] Clock check: %llu ms\n",(unsigned long long)elapsed);
        if(elapsed<10||elapsed>1000)std::fprintf(stderr,"Clock check failed; game not started.\n");
        else{
            std::string error;auto cpu=Cpu::CreateNative(&error);
            if(!cpu)std::fprintf(stderr,"CPU: %s\n",error.c_str());
            else{
                S3EBinary binary;std::string assets=std::string(ROOT)+"/game";
                if(LoadS3E(assets+"/pandemonium.xe3u",binary)&&SetupSandbox(cpu.get(),binary,STOCK_BASE,64u<<20)){
                    bool pointersOK=true;for(const auto& p:binary.pointer_fixups){u32 actual=0;cpu->MemRead(G(binary.base_addr+p.offset),&actual,4);if(actual!=TRAMPOLINE_BASE+p.func_idx*8)pointersOK=false;}
                    printf("[3DS] Type-4 function pointers: %s (%u)\n",pointersOK?"verified":"FAILED",(unsigned)binary.pointer_fixups.size());
                    if(pointersOK && !EnablePandemoniumMusicInSfx(cpu.get())){
                        pointersOK=false;
                        ActivateFaultConsole();
                        fprintf(stderr,"Unsupported Pandemonium game image.\n");
                    }
                    if(pointersOK){
                        S3ERuntime runtime(cpu.get(),assets,std::string(ROOT)+"/user");runtime.Init(binary.imports,binary.config);
                        Audio3DS audio;audio.Init();bool suppressAUntilRelease=false;
                        runtime.SetVideoPlayer([&](const std::string& path,const S3ERuntime::VideoRequest& request){
                            audio.Shutdown();bottomPrompts=1;DrawBottomArt();
                            int result=1;try{result=PlayPandemoniumMovie(path,request);}catch(const std::exception& e){fprintf(stderr,"[CinematicError] %s\n",e.what());}
                            bottomPrompts=0;DrawBottomArt();audio.Init();hidScanInput();suppressAUntilRelease=(hidKeysHeld()&KEY_A)!=0;
                            if(!aptMainLoop())runtime.RequestQuit();
                            return result==0?0:-1;
                        });
                        runtime.SetAudioCallback([&](const int16_t* p,size_t n,uint32_t rate){audio.Push(p,n,rate);});
                        puts("D-pad / Circle Pad: directions\nA: CENTER / confirm\nB: right soft key / back\nSTART: pause\nX: keypad 5; Y: keypad 0; L: character switch; R: keypad 9\n");
                        runtime.SetPollCallback([&](S3ERuntime::Keys& keys){
                            keys.fill(false);if(!aptMainLoop()){runtime.RequestQuit();return;}hidScanInput();u32 held=hidKeysHeld();if(suppressAUntilRelease){if(!(held&KEY_A))suppressAUntilRelease=false;held&=~KEY_A;}
                            keys[9]=(held&(KEY_DLEFT|KEY_CPAD_LEFT))!=0;keys[10]=(held&(KEY_DUP|KEY_CPAD_UP))!=0;keys[11]=(held&(KEY_DRIGHT|KEY_CPAD_RIGHT))!=0;keys[12]=(held&(KEY_DDOWN|KEY_CPAD_DOWN))!=0;
                            keys[78]=(held&KEY_A)!=0;keys[210]=(held&KEY_B)!=0;keys[73]=(held&KEY_START)!=0;keys[18]=(held&KEY_X)!=0;keys[13]=(held&KEY_Y)!=0;keys[77]=(held&KEY_L)!=0;keys[22]=(held&KEY_R)!=0;
                        });
                        runtime.SetFrameCallback([&](uint32_t,const uint16_t* pixels,S3ERuntime::Keys&){
                            const auto prompts=runtime.TakeConsolePrompts();
                            if(!firstGameFrame||prompts!=bottomPrompts){firstGameFrame=true;bottomPrompts=prompts;DrawBottomArt();}
                            u8* screen=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,nullptr,nullptr);memset(screen,0,400*240*3);
                            for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x){u16 p=pixels[y*320+x];u8* out=screen+((x+40)*240+239-y)*3;out[0]=(p&31)*255/31;out[1]=((p>>5)&63)*255/63;out[2]=((p>>11)&31)*255/31;}
                            gfxFlushBuffers();gfxScreenSwapBuffers(GFX_TOP,false);gspWaitForVBlank();
                        });
                        result=runtime.Run(G(binary.entry_point));if(runtime.Failed())result=1;
                    }
                }
            }
        }
    }
    }catch(const std::bad_alloc&){
        result=1;ActivateFaultConsole();fprintf(stderr,"[HostFailure] Host memory allocation failed.\n");
    }catch(const std::exception& e){
        result=1;ActivateFaultConsole();fprintf(stderr,"[HostFailure] %s\n",e.what());
    }
    if(result!=0){ActivateFaultConsole();std::fprintf(stderr,"\nUnable to continue (runtime result %d).\n",result);WaitAfterError();}
    fflush(nullptr);devoptab_list[STD_OUT]=originalOutput;devoptab_list[STD_ERR]=originalError;
    aptUnhook(&appHookCookie);gfxExit();return result;
}
