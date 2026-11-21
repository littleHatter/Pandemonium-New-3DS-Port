// Standalone player adaptation; MIT terms in third_party/movie_player_LICENSE.
#include <3ds.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <exception>
#include <sys/stat.h>
#include "decoder_api.h"
#include "movie_player.h"
namespace {
S3ERuntime::VideoRequest displayRequest{};
uint64_t Ticks(){return svcGetSystemTick();}
double Seconds(uint64_t ticks){return double(ticks)/SYSCLOCK_ARM11;}
void Log(const char* format,...){char line[1024];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);printf("[Cinematic] %s",line);}

struct Timings {uint64_t count=0,ticks=0,maxTicks=0;void Add(uint64_t elapsed){++count;ticks+=elapsed;maxTicks=std::max(maxTicks,elapsed);}};
class Audio {
 static constexpr unsigned Count=16,Block=1152;
 std::array<ndspWaveBuf,Count> buffers{};
 std::array<uint64_t,Count> first{};
 s16* memory=nullptr;unsigned next=0;uint64_t lastClock=0;bool initialized=false;
public:
 unsigned rate=0;uint64_t samples=0,clipped=0;bool started=false,paused=true;
 bool Init(unsigned sampleRate){rate=sampleRate;Result rc=ndspInit();if(R_FAILED(rc)){Log("[AudioError] ndspInit=%08lx\n",(unsigned long)rc);return false;}initialized=true;memory=(s16*)linearAlloc(Count*Block*2);if(!memory){Log("[AudioError] PCM allocation failed\n");return false;}
  ndspChnReset(0);ndspChnSetInterp(0,NDSP_INTERP_LINEAR);ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM16);ndspChnSetRate(0,float(rate));float mix[12]{};mix[0]=mix[1]=1;ndspChnSetMix(0,mix);ndspChnSetPaused(0,true);
  for(unsigned i=0;i<Count;++i)buffers[i].data_pcm16=memory+i*Block;
  return true;
 }
 unsigned Queued()const{unsigned n=0;for(const auto& b:buffers)if(b.status==NDSP_WBUF_QUEUED||b.status==NDSP_WBUF_PLAYING)++n;return n;}
 bool CanPush()const{const auto s=buffers[next].status;return s!=NDSP_WBUF_QUEUED&&s!=NDSP_WBUF_PLAYING;}
 bool Push(plm_samples_t* a){if(!memory||!a||a->count>Block||!CanPush())return false;auto& b=buffers[next];
  for(unsigned i=0;i<a->count;++i){float value=(a->interleaved[2*i]+a->interleaved[2*i+1])*16384.0f*(float(displayRequest.volume)/256.0f);if(!std::isfinite(value))value=0;if(value>32767||value<-32768)++clipped;b.data_pcm16[i]=int16_t(std::clamp(value,-32768.0f,32767.0f));}
  DSP_FlushDataCache(b.data_pcm16,a->count*2);b.nsamples=a->count;b.offset=0;b.looping=false;b.next=nullptr;b.status=NDSP_WBUF_FREE;first[next]=samples;samples+=a->count;ndspChnWaveBufAdd(0,&b);next=(next+1)%Count;return true;
 }
 void Pause(bool value){paused=value;if(initialized)ndspChnSetPaused(0,value);}
 void Start(){started=true;Pause(false);}
 uint64_t Clock(){if(!started)return 0;const u16 seq=ndspChnGetWaveBufSeq(0);const u32 position=ndspChnGetSamplePos(0);
  for(unsigned i=0;i<Count;++i){const auto& b=buffers[i];if(b.sequence_id==seq&&(b.status==NDSP_WBUF_QUEUED||b.status==NDSP_WBUF_PLAYING))lastClock=std::max(lastClock,first[i]+std::min(position,b.nsamples));}
  if(!Queued())lastClock=samples;
  return lastClock;
 }
 ~Audio(){if(initialized){ndspChnWaveBufClear(0);ndspChnReset(0);ndspExit();}if(memory)linearFree(memory);}
};
struct Lifecycle {Audio* audio=nullptr;bool paused=false,refresh=true;uint64_t pauseStart=0,pausedTicks=0;unsigned suspends=0;
 void Pause(bool value){if(value==paused)return;paused=value;if(value){pauseStart=Ticks();++suspends;}else{pausedTicks+=Ticks()-pauseStart;refresh=true;}if(audio)audio->Pause(value);}
};
void AppEvent(APT_HookType event,void* ptr){auto& s=*static_cast<Lifecycle*>(ptr);if(event==APTHOOK_ONSUSPEND||event==APTHOOK_ONSLEEP)s.Pause(true);if(event==APTHOOK_ONRESTORE||event==APTHOOK_ONWAKEUP)s.Pause(false);}
class AptHookGuard{aptHookCookie cookie;bool active=true;public:explicit AptHookGuard(Lifecycle* state){aptHook(&cookie,AppEvent,state);}void Reset(){if(active){aptUnhook(&cookie);active=false;}}~AptHookGuard(){Reset();}};
void Present(const uint8_t* rgb,unsigned w,unsigned h){u8* screen=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,nullptr,nullptr);memset(screen,0,400*240*3);const unsigned canvas=displayRequest.canvasWidth==400?400:320;
 unsigned left=(400-canvas)/2+displayRequest.left,top=displayRequest.top,dw=displayRequest.width,dh=displayRequest.height;
 if(!dw||!dh||displayRequest.left>canvas||displayRequest.top>240||dw>canvas-displayRequest.left||dh>240-displayRequest.top){left=(400-w)/2;top=(240-h)/2;dw=w;dh=h;}
 for(unsigned y=0;y<dh;++y)for(unsigned x=0;x<dw;++x){const auto* p=rgb+3*((y*h/dh)*w+x*w/dw);auto* out=screen+((left+x)*240+239-top-y)*3;out[0]=p[2];out[1]=p[1];out[2]=p[0];}

 GSPGPU_FlushDataCache(screen,400*240*3);gfxScreenSwapBuffers(GFX_TOP,false);
}
int Play(const char* path){
 struct stat st{};if(stat(path,&st)||st.st_size<16||st.st_size>(64<<20)){Log("[Error] Missing/invalid movie: %s\n",path);return 1;}
 plm_t* decoder=plm_create_with_filename(path);if(!decoder){Log("[Error] Unable to open MPEG stream\n");return 1;}
 std::unique_ptr<plm_t,decltype(&plm_destroy)> owner(decoder,&plm_destroy);
 unsigned width=plm_get_width(decoder),height=plm_get_height(decoder),rate=plm_get_samplerate(decoder);double fps=plm_get_framerate(decoder),duration=plm_get_duration(decoder);
 Log("[Media] bytes=%lu dimensions=%ux%u fps=%.6f audio=%u Hz duration=%.6f\n",(unsigned long)st.st_size,width,height,fps,rate,duration);
 if(!width||!height||width>400||height>240||!std::isfinite(fps)||fps<1||fps>60||rate<8000||rate>48000||!std::isfinite(duration)||duration<=0||duration>600){Log("[Error] Unsupported media header\n");owner.reset();return 1;}
 std::vector<uint8_t> rgb(width*height*3);
 Audio audio;if(!audio.Init(rate)){owner.reset();return 1;}
 Lifecycle lifecycle;lifecycle.audio=&audio;AptHookGuard hook(&lifecycle);
 Timings audioTime,videoTime,convertTime;uint64_t videoFrames=0,presentations=0,lateFrames=0,underruns=0;double maxLateness=0,videoEnd=0,audioEnd=0;bool audioEof=false,starved=false,hasImage=false,tailStarted=false;uint64_t tailStart=0,tailPausedAt=0;double tailBase=0,lastStatus=0,lastMaster=0;
 auto FillAudio=[&](){while(!audioEof&&audio.Queued()<8&&audio.CanPush()){uint64_t before=Ticks();auto* frame=plm_decode_audio(decoder);audioTime.Add(Ticks()-before);if(!frame){audioEof=true;break;}audioEnd=frame->time+double(frame->count)/rate;if(!audio.Push(frame)){Log("[Error] Audio buffer/format rejected\n");return false;}}return true;};
 auto NextVideo=[&](){uint64_t before=Ticks();auto* frame=plm_decode_video(decoder);videoTime.Add(Ticks()-before);return frame;};
 const char* reason="error";int status=1;
 if(!FillAudio()||!audio.Queued()){Log("[Error] No decoded audio\n");hook.Reset();owner.reset();return 1;}
 auto* next=NextVideo();if(!next){Log("[Error] No decoded video\n");hook.Reset();owner.reset();return 1;}
 Log("[Playback] Start; A skips. HOME supported. Gain=unity. Prefilled=%u blocks\n",audio.Queued());
 bool skipArmed=false;audio.Start();const uint64_t start=Ticks();const double guard=std::min(620.0,duration+20.0);
 while(aptMainLoop()){
  hidScanInput();if(!(hidKeysHeld()&KEY_A))skipArmed=true;if(skipArmed&&(hidKeysDown()&KEY_A)){reason="skipped";status=0;break;}
  unsigned queued=audio.Queued();if(!audioEof&&queued==0){if(!starved)++underruns;starved=true;}else starved=false;
  if(!FillAudio())break;
  double master=double(audio.Clock())/rate;
  if(audioEof&&audio.Queued()==0){if(!tailStarted){tailStarted=true;tailStart=Ticks();tailBase=master;tailPausedAt=lifecycle.pausedTicks;}master=tailBase+Seconds(Ticks()-tailStart-(lifecycle.pausedTicks-tailPausedAt));}
  master=std::max(lastMaster,master);lastMaster=master;
  unsigned due=0;
  while(next&&next->time<=master+0.0005&&due<8){
   maxLateness=std::max(maxLateness,master-next->time);if(master-next->time>1.5/fps)++lateFrames;
   uint64_t before=Ticks();plm_frame_to_rgb(next,rgb.data(),width*3);convertTime.Add(Ticks()-before);
   videoEnd=next->time+1/fps;hasImage=true;++videoFrames;++due;next=NextVideo();
  }
  if(hasImage&&(due||lifecycle.refresh)){Present(rgb.data(),width,height);++presentations;lifecycle.refresh=false;}
  double elapsed=Seconds(Ticks()-start-lifecycle.pausedTicks);
  if(elapsed-lastStatus>=10){lastStatus=elapsed;auto m=GetDecoderMemoryStats();Log("[Progress] active=%.3f audioClock=%.3f frame=%llu queued=%u underruns=%llu late=%llu decoderPeak=%lu\n",elapsed,master,(unsigned long long)videoFrames,audio.Queued(),(unsigned long long)underruns,(unsigned long long)lateFrames,(unsigned long)m.peak);}
  if(!next&&audioEof&&audio.Queued()==0&&master>=videoEnd){reason="completed";status=0;break;}
  if(elapsed>guard){Log("[Error] Playback time guard reached\n");reason="timeout";break;}
  gspWaitForVBlank();
 }
 if(status&&strcmp(reason,"error")==0)reason="home_exit_or_error";
 const double elapsed=Seconds(Ticks()-start-lifecycle.pausedTicks);audio.Pause(true);hook.Reset();
 auto memory=GetDecoderMemoryStats();owner.reset();auto after=GetDecoderMemoryStats();
 Log("[Result] reason=%s activeSeconds=%.6f videoFrames=%llu presents=%llu videoEnd=%.6f audioEnd=%.6f audioSamples=%llu clock=%.6f underruns=%llu lateFrames=%llu maxLatenessMs=%.3f clippedSamples=%llu suspends=%u\n",reason,elapsed,(unsigned long long)videoFrames,(unsigned long long)presentations,videoEnd,audioEnd,(unsigned long long)audio.samples,lastMaster,(unsigned long long)underruns,(unsigned long long)lateFrames,1000*maxLateness,(unsigned long long)audio.clipped,lifecycle.suspends);
 for(auto pair:{std::pair<const char*,Timings>{"video",videoTime},{"audio",audioTime},{"convert",convertTime}})Log("[Timing] %s calls=%llu meanMs=%.4f maxMs=%.4f\n",pair.first,(unsigned long long)pair.second.count,pair.second.count?1000*Seconds(pair.second.ticks)/pair.second.count:0,1000*Seconds(pair.second.maxTicks));
 Log("[Memory] decoderPeak=%lu largestAllocation=%lu reallocations=%llu decoderLiveAfterDestroy=%lu linearPCM=%u rgbFrame=%u\n",(unsigned long)memory.peak,(unsigned long)memory.largest,(unsigned long long)memory.reallocations,(unsigned long)after.live,16*1152*2,width*height*3);
 return status;
}
}
int PlayPandemoniumMovie(const std::string& path,const S3ERuntime::VideoRequest& request){displayRequest=request;return Play(path.c_str());}
