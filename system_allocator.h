#ifndef SYSTEM_ALLOCATOR_H
#define SYSTEM_ALLOCATOR_H

#include "allocator.h"

struct SystemAllocator {
    struct Allocator allocator;
};


struct SystemAllocator make_system_allocator(struct AllocatorOptions options);

#endif

