#ifndef ALLOCATOR_BASE_H
#define ALLOCATOR_BASE_H

#include <stdint.h>

typedef void* __attribute__((aligned(16))) Allocator;

struct Memory {
    void*  base;
    size_t size;
};
typedef struct Memory Memory;
#define MEMORY_NULL ((Memory){ 0 })

typedef Memory (*AllocateFn)(void* allocator, size_t size, size_t alignment);
typedef Memory (*ReallocateFn)(void* allocator, Memory memory, size_t new_size, size_t alignment);
typedef void   (*DeallocateFn)(void* allocator, Memory memory);
typedef void   (*DestroyFn)(void* allocator);


#define allocator_to_base(allocator, kind) (Allocator)((uintptr_t)(allocator) | (kind));


#endif  // ALLOCATOR_BASE_H