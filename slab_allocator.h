#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include "allocator.h"

struct SlabNode;

struct SlabAllocator {
    struct Allocator allocator;
    struct SlabNode* slabs;
    size_t slot_size;
    size_t slot_stride;
    uint32_t slots_per_slab;
    uint32_t slab_count;
};

struct SlabAllocatorOptions {
    struct AllocatorOptions allocator_options;
    size_t slot_size;
    uint32_t slots_per_slab;
};

struct SlabAllocator make_slab_allocator(struct SlabAllocatorOptions options);

#endif

