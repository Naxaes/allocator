#ifndef SYSTEM_ALLOCATOR_H
#define SYSTEM_ALLOCATOR_H

#include "allocator.h"

struct SystemAllocator {
    struct Allocator allocator;
};

struct SystemAllocatorOptions {
    const char* name;
    struct AllocatorHandle parent;
    uint8_t oom_strategy;
    uint32_t alignment;
};

struct SystemAllocator make_system_allocator(struct SystemAllocatorOptions options);

#endif

