#ifndef STACK_ALLOCATOR_H
#define STACK_ALLOCATOR_H

#include "allocator.h"

struct StackAllocator {
    struct Allocator allocator;
    uint8_t *pool;
    size_t pool_size;
    size_t used;
};

struct StackAllocator make_stack_allocator(struct AllocatorHandle parent, uint8_t oom_strategy);

#endif

