#ifndef POOL_ALLOCATOR_H
#define POOL_ALLOCATOR_H

#include "allocator.h"

struct PoolAllocator {
    struct Allocator allocator;
    struct MemoryRegion backing_region;
    void*  pool_start;
    void*  free_list;
    uint32_t slot_size;
    uint32_t slot_stride;
    uint32_t capacity;
    uint32_t free_count;
};

struct PoolAllocatorOptions {
    struct AllocatorOptions allocator_options;
    uint32_t slot_size;
    uint32_t capacity;
};

struct PoolAllocator make_pool_allocator(struct PoolAllocatorOptions options);

#endif

