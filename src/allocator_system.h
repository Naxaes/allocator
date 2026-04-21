#ifndef ALLOCATOR_SYSTEM_H
#define ALLOCATOR_SYSTEM_H

#include "allocators.h"


Memory allocate_system(Allocator* allocator, size_t size, size_t alignment);
Memory reallocate_system(Allocator* allocator, Memory memory, size_t new_size, size_t alignment);
void   deallocate_system(Allocator* allocator, Memory memory);
void   destroy_system(Allocator* allocator);

#endif


#ifdef ALLOCATOR_SYSTEM_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

inline Memory allocate_system(Allocator* allocator, size_t size, size_t alignment) {
    (void)allocator;
    void* ptr = NULL;
    int result = posix_memalign(&ptr, alignment, size);
    if (result != 0) {
        return MEMORY_NULL;
    }
    return (Memory) { .base = ptr, .size = size };
}

inline Memory reallocate_system(Allocator* allocator, Memory memory, size_t new_size, size_t alignment) {
    (void)allocator;
    void* new_base = NULL;
    int result = posix_memalign(&new_base, alignment, new_size);
    if (result != 0) {
        return MEMORY_NULL;
    }
    if (memory.base != NULL) {
        size_t copy_size = memory.size < new_size ? memory.size : new_size;
        memcpy(new_base, memory.base, copy_size);
        free(memory.base);
    }
    return (Memory) { .base = new_base, .size = new_size };
}

inline void deallocate_system(Allocator* allocator, Memory memory) {
    (void)allocator;
    free(memory.base);
}

inline void destroy_system(Allocator* allocator) {
    (void)allocator;
}

size_t query_system(const Allocator* allocator, AllocatorQuery query) {
    (void)allocator;
    switch (query) {
        case ALLOCATOR_QUERY_GET_PARENT:
            return (size_t)0;
        default:
            return 0;
    }
}


#endif  // ALLOCATOR_SYSTEM_IMPLEMENTATION