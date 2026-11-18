#include <cstdlib>
#include <new>
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include "asset_read.h"
#include <zlib.h>
static size_t live=0,peak=0,budget=24u<<20;
struct alignas(std::max_align_t) Header{size_t n;};
void* operator new(size_t n){if(live+n>budget){printf("denied allocation=%zu live=%zu required=%zu\n",n,live,live+n);throw std::bad_alloc();}auto p=(Header*)malloc(n+sizeof(Header));if(!p)throw std::bad_alloc();p->n=n;live+=n;if(live>peak)peak=live;return p+1;}
void operator delete(void* v) noexcept{if(v){auto p=(Header*)v-1;live-=p->n;free(p);}}
void operator delete(void* v,size_t) noexcept{operator delete(v);}
int main(int argc,char**argv){bool exact=argc>2;try{std::vector<char> image(1963128);std::ifstream f(argv[1],std::ios::binary|(exact?std::ios::ate:std::ios::openmode(0)));std::vector<uint8_t> data;if(exact){f.close();if(!ReadAssetExact(argv[1],data))return 2;}else data.assign(std::istreambuf_iterator<char>(f),{});printf("mode=%s bytes=%zu peak=%zu budget=%zu crc32=%08lx result=PASS\n",exact?"exact":"iterator",data.size(),peak,budget,(unsigned long)crc32(0,data.data(),data.size()));}catch(const std::bad_alloc&){printf("mode=%s peak=%zu budget=%zu result=BAD_ALLOC\n",exact?"exact":"iterator",peak,budget);return 1;}}
