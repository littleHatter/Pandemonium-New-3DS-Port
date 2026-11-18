#include "runtime_s3e.h"
#include "loader.h"
#include <cassert>
#include <fstream>
#include <algorithm>
#include <cstdio>
int main(int argc,char**argv){
 if(argc!=2)return 2;std::string root=argv[1],error;auto cpu=Cpu::CreateUnicorn(&error);assert(cpu);
 S3EBinary bin;assert(LoadS3E(root+"/pandemonium.xe3u",bin));for(const char* name:{"s3eSoundChannelPause","s3eSoundChannelResume"})if(std::find(bin.imports.begin(),bin.imports.end(),name)==bin.imports.end())bin.imports.push_back(name);assert(SetupSandbox(cpu.get(),bin));assert(cpu->MemMap(0x60000000,4096)==0);
 uint32_t callback[]={0xe59f200c,0xe5923000,0xe2833001,0xe5823000,0xe12fff1e,0x60000100};cpu->MemWrite(0x60000040,callback,sizeof(callback));
 S3ERuntime runtime(cpu.get(),root,"/tmp/pan-abi-probe-saves");runtime.Init(bin.imports,bin.config);
 unsigned plays=0;int result=0;std::string last;
 runtime.SetVideoPlayer([&](const std::string& path,const S3ERuntime::VideoRequest& request){++plays;last=path;assert(request.volume==250);return result;});
 auto word=[&](uint32_t at){uint32_t value=0;assert(cpu->MemRead(at,&value,4)==0);return value;};
 auto invoke=[&](const char* name,uint32_t a=0,uint32_t b=0,uint32_t c=0,uint32_t d=0){
  auto it=std::find(bin.imports.begin(),bin.imports.end(),name);if(it==bin.imports.end())fprintf(stderr,"missing test import: %s\n",name);assert(it!=bin.imports.end());uint32_t args[]={a,b,c,d};for(int i=0;i<4;++i)cpu->RegWrite(i,args+i);
  uint32_t sp=STACK_BASE+STACK_SIZE-256,lr=0x60000080,flags=0x10;cpu->RegWrite(Cpu::REG_CPSR,&flags);cpu->RegWrite(Cpu::REG_SP,&sp);cpu->RegWrite(Cpu::REG_LR,&lr);uint32_t stack[]={3,201,165,0};cpu->MemWrite(sp,stack,sizeof(stack));
  assert(cpu->Start(TRAMPOLINE_BASE+8*(it-bin.imports.begin()),lr,10000000)==0);uint32_t value=0;cpu->RegRead(0,&value);assert(!runtime.Failed());return value;
 };
 invoke("s3eSoundChannelPlay",0,HEAP_BASE+16,16,1);assert(invoke("s3eSoundChannelGetInt",0,4)==1);
 invoke("s3eSoundChannelPause",0);assert(invoke("s3eSoundChannelGetInt",0,4)==1);
 invoke("s3eSoundChannelResume",0);assert(invoke("s3eSoundChannelGetInt",0,4)==1);
 invoke("s3eSoundChannelStop",0);assert(invoke("s3eSoundChannelGetInt",0,4)==0);
 assert(invoke("s3eVideoIsCodecSupported",1)==1);assert(invoke("s3eVideoIsCodecSupported",2)==0);invoke("s3eVideoRegister",0,0x60000040,0);invoke("s3eVideoSetInt",0,250);
 for(unsigned pass=0;pass<3;++pass){
  const char* file=pass==1?"out_mpg4.dz":"in_mpg4.dz";std::ifstream f(root+"/"+file,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});assert(!bytes.empty());cpu->MemWrite(HEAP_BASE+0x10000,bytes.data(),bytes.size());
  result=pass==0?0:pass==1?1:-1;
  assert(invoke("s3eVideoPlayFromBuffer",HEAP_BASE+0x10000,bytes.size(),1,59)==0);assert(plays==pass);
  assert(invoke("s3eVideoGetInt",1)==0);assert(plays==pass+1);assert(word(0x60000100)==pass+1);assert(last.find(pass==1?"ending.mpg":"intro.mpg")!=std::string::npos);
  invoke("s3eVideoStop");invoke("s3eVideoPlayFromBuffer",HEAP_BASE+0x10000,bytes.size(),1,59);invoke("s3eVideoStop");assert(invoke("s3eVideoGetInt",1)==0);assert(plays==pass+1);assert(word(0x60000100)==pass+1);
 }
 cpu->MemWrite(HEAP_BASE+0x10000,"BAD!",4);assert(invoke("s3eVideoPlayFromBuffer",HEAP_BASE+0x10000,4,1,59)==1);assert(plays==3);
 assert(invoke("s3eMemoryHeapDestroy",1,1)==1);assert(invoke("s3eMemoryHeapCreate",1)==0);assert(invoke("s3eMemoryHeapAddress",1)==0);assert(invoke("s3eMemoryHeapDestroy",1,1)==0);
 puts("PASS: channel playing/paused/stop queries; intro/ending mapping; completion, skip, backend failure; exactly-once callbacks; cleanup play/stop cancellation; unknown media rejection; logical shared-heap lifecycle");
}
