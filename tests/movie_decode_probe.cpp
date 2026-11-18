#include "decoder_api.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <algorithm>
int main(int argc,char**argv){
 if(argc<2)return 2;
 plm_t* p=plm_create_with_filename(argv[1]);if(!p){puts("open failed");return 2;}
 const unsigned width=plm_get_width(p),height=plm_get_height(p),rate=plm_get_samplerate(p);const double fps=plm_get_framerate(p),duration=plm_get_duration(p);
 if(!width||!height||width>400||height>240||rate<8000||rate>48000||fps<=0){puts("invalid streams");plm_destroy(p);return 2;}
 std::vector<uint8_t> rgb(width*height*3);uint64_t frames=0,audioSamples=0,clipped=0;double videoEnd=0,audioEnd=0,peak=0;bool audioEnded=false;FILE* pcm=argc>2?fopen(argv[2],"wb"):nullptr;
 auto audio=[&](){auto* a=plm_decode_audio(p);if(!a){audioEnded=true;return;}audioSamples+=a->count;audioEnd=a->time+double(a->count)/rate;
  for(unsigned i=0;i<a->count;++i){double f=(a->interleaved[2*i]+a->interleaved[2*i+1])*0.5;peak=std::max(peak,std::abs(f));int value=int(f*32768.0);if(value>32767||value<-32768)++clipped;value=std::clamp(value,-32768,32767);int16_t s=value;if(pcm)fwrite(&s,2,1,pcm);}
 };
 while(auto* v=plm_decode_video(p)){
  ++frames;videoEnd=v->time+1/fps;
  if(frames>20000){puts("frame guard");return 3;}
  if(frames==1||frames==1000||frames==2000){plm_frame_to_rgb(v,rgb.data(),width*3);if(argc>3){char name[512];snprintf(name,sizeof(name),"%s_%llu.ppm",argv[3],(unsigned long long)frames);FILE* image=fopen(name,"wb");if(image){fprintf(image,"P6\n%u %u\n255\n",width,height);fwrite(rgb.data(),1,rgb.size(),image);fclose(image);}}}
  while(!audioEnded&&audioEnd<videoEnd+.3)audio();
 }
 while(!audioEnded)audio();
 if(pcm)fclose(pcm);auto stats=GetDecoderMemoryStats();plm_destroy(p);auto after=GetDecoderMemoryStats();
 printf("{\"width\":%u,\"height\":%u,\"fps\":%.6f,\"rate\":%u,\"reported_duration\":%.6f,\"video_frames\":%llu,\"video_end\":%.6f,\"audio_samples\":%llu,\"audio_end\":%.6f,\"unity_gain_clipped_samples\":%llu,\"unity_gain_peak\":%.6f,\"decoder_peak_bytes\":%zu,\"decoder_largest_allocation\":%zu,\"decoder_reallocations\":%llu,\"decoder_live_after_destroy\":%zu}\n",width,height,fps,rate,duration,(unsigned long long)frames,videoEnd,(unsigned long long)audioSamples,audioEnd,(unsigned long long)clipped,peak,stats.peak,stats.largest,(unsigned long long)stats.reallocations,after.live);
 return frames&&audioSamples&&!after.live?0:1;
}
