#include <cstdlib>
#include <cstddef>
#include <algorithm>
#include <new>
#include "decoder_api.h"
namespace {
struct alignas(std::max_align_t) Allocation{size_t bytes;};
DecoderMemoryStats stats;
void* MovieMalloc(size_t n){if(n>(4u<<20)||stats.live+n>(4u<<20))throw std::bad_alloc();auto* p=(Allocation*)std::malloc(sizeof(Allocation)+n);if(!p)throw std::bad_alloc();p->bytes=n;stats.live+=n;stats.peak=std::max(stats.peak,stats.live);stats.largest=std::max(stats.largest,n);++stats.allocations;return p+1;}
void MovieFree(void* p){if(!p)return;auto* a=(Allocation*)p-1;stats.live-=a->bytes;std::free(a);}
void* MovieRealloc(void* p,size_t n){if(!p)return MovieMalloc(n);auto* a=(Allocation*)p-1;size_t old=a->bytes;if(n>(4u<<20)||stats.live-old+n>(4u<<20))throw std::bad_alloc();auto* q=(Allocation*)std::realloc(a,sizeof(Allocation)+n);if(!q)throw std::bad_alloc();q->bytes=n;stats.live=stats.live-old+n;stats.peak=std::max(stats.peak,stats.live);stats.largest=std::max(stats.largest,n);++stats.reallocations;return q+1;}
}
DecoderMemoryStats GetDecoderMemoryStats(){return stats;}
#define PLM_MALLOC MovieMalloc
#define PLM_FREE MovieFree
#define PLM_REALLOC MovieRealloc
#define PL_MPEG_IMPLEMENTATION
#include "third_party/pl_mpeg.h"
