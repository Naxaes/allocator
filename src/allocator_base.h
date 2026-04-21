#ifndef ALLOCATOR_BASE_H
#define ALLOCATOR_BASE_H

#include <stddef.h>


#define ALLOCATOR_KINDS_COUNT 8
#define ALLOCATOR_KIND_TAG_MASK (ALLOCATOR_KINDS_COUNT - 1)
#define ALLOCATOR_KIND_DATA_MASK (~ALLOCATOR_KIND_TAG_MASK)


struct Allocator;
typedef struct Allocator __attribute__((aligned(ALLOCATOR_KINDS_COUNT))) Allocator;

struct Memory {
    void*  base;
    size_t size;
};
typedef struct Memory Memory;
#define MEMORY_NULL ((Memory){ 0 })


typedef Memory (*AllocateFn)(Allocator* allocator, size_t size, size_t alignment);
typedef Memory (*ReallocateFn)(Allocator* allocator, Memory memory, size_t new_size, size_t alignment);
typedef int    (*DeallocateFn)(Allocator* allocator, Memory memory);
typedef void   (*DestroyFn)(Allocator* allocator);


#define allocator_to_base(allocator, kind) (Allocator*)((uintptr_t)(allocator) | (kind));


#endif  // ALLOCATOR_BASE_H
