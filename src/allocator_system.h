#ifndef ALLOCATOR_SYSTEM_H
#define ALLOCATOR_SYSTEM_H

#include "allocators.h"

int  allocator_system_init(void);
void allocator_system_deinit(void);

Memory allocate_system(Allocator* allocator, size_t size, size_t alignment);
Memory reallocate_system(Allocator* allocator, Memory memory, size_t new_size, size_t alignment);
int    deallocate_system(Allocator* allocator, Memory memory);
void   destroy_system(Allocator* allocator);

#endif


#ifdef ALLOCATOR_SYSTEM_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

int allocator_system_init(void) {
    int kind = allocator_register_kind((AllocatorFunctionTable) {
        .allocate = allocate_system,
        .reallocate = reallocate_system,
        .deallocate = deallocate_system,
        .destroy = destroy_system,
    });
    assert(kind == 0 && "System allocator must be registered as the first allocator kind");
    allocator_push(0);
    return kind;
}

void allocator_system_deinit(void) {
    allocator_pop();
}

Memory allocate_system(Allocator* allocator, size_t size, size_t alignment) {
    (void)allocator;
    void* ptr = NULL;
    int result = posix_memalign(&ptr, alignment, size);
    if (result != 0) {
        return MEMORY_NULL;
    }
    return (Memory) { .base = ptr, .size = size };
}

Memory reallocate_system(Allocator* allocator, Memory memory, size_t new_size, size_t alignment) {
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

int deallocate_system(Allocator* allocator, Memory memory) {
    (void)allocator;
    free(memory.base);
    return 1;
}

void destroy_system(Allocator* allocator) {
    (void)allocator;
}



#endif  // ALLOCATOR_SYSTEM_IMPLEMENTATION
