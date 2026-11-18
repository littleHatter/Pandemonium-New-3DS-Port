#pragma once
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include "third_party/pl_mpeg.h"
struct DecoderMemoryStats{size_t live=0,peak=0,largest=0;uint64_t allocations=0,reallocations=0;};
DecoderMemoryStats GetDecoderMemoryStats();
