#ifndef ALLOCATORS_ALLOCATORS_H
#define ALLOCATORS_ALLOCATORS_H
#include <stddef.h>
#include <stdint.h>







typedef struct Memory Memory;
typedef void* __attribute__((aligned(16))) Allocator;








typedef struct AllocatorFunctionTable AllocatorFunctionTable;





struct Memory {
    void*  base;
    size_t size;
};
#define MEMORY_NULL ((Memory){ 0 })

typedef Memory (*AllocateFn)(void* allocator, size_t size, size_t alignment);
typedef Memory (*ReallocateFn)(void* allocator, Memory memory, size_t new_size, size_t alignment);
typedef void   (*DeallocateFn)(void* allocator, Memory memory);
typedef void   (*DestroyFn)(void* allocator);

struct AllocatorFunctionTable {
    AllocateFn   allocate;
    ReallocateFn reallocate;
    DeallocateFn deallocate;
    DestroyFn    destroy;
} allocator_function_kinds[16];
static int allocator_function_kinds_count = 0;



#if ALLOCATOR_DEBUG
/* User options */
#ifndef ALLOCATOR_PRE_ALLOCATE_HOOK
#define ALLOCATOR_PRE_ALLOCATE_HOOK(allocator, size, alignment) ((void)0)
#endif
#ifndef ALLOCATOR_POST_ALLOCATE_HOOK
#define ALLOCATOR_POST_ALLOCATE_HOOK(allocator, size, alignment, result) ((void)0)
#endif

#ifndef ALLOCATOR_PRE_REALLOCATE_HOOK
#define ALLOCATOR_PRE_REALLOCATE_HOOK(allocator, memory, new_size, alignment) ((void)0)
#endif
#ifndef ALLOCATOR_POST_REALLOCATE_HOOK
#define ALLOCATOR_POST_REALLOCATE_HOOK(allocator, memory, new_size, alignment, result) ((void)0)
#endif

#ifndef ALLOCATOR_PRE_DEALLOCATE_HOOK
#define ALLOCATOR_PRE_DEALLOCATE_HOOK(allocator, memory) ((void)0)
#endif
#ifndef ALLOCATOR_POST_DEALLOCATE_HOOK
#define ALLOCATOR_POST_DEALLOCATE_HOOK(allocator, memory) ((void)0)
#endif

#ifndef ALLOCATOR_PRE_DESTROY_HOOK
#define ALLOCATOR_PRE_DESTROY_HOOK(allocator) ((void)0)
#endif
#ifndef ALLOCATOR_POST_DESTROY_HOOK
#define ALLOCATOR_POST_DESTROY_HOOK(allocator) ((void)0)
#endif

static inline Memory allocator_alloc(Allocator allocator, size_t size, size_t alignment) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_ALLOCATE_HOOK(allocator, size, alignment);
    Memory result = table.allocate((void*)data, size, alignment);
    ALLOCATOR_POST_ALLOCATE_HOOK(allocator, size, alignment, result);

    return result;
}

static inline Memory allocator_realloc(Allocator allocator, Memory memory, size_t new_size, size_t alignment) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_REALLOCATE_HOOK(allocator, memory, new_size, alignment);
    Memory result = table.reallocate((void*)data, memory, new_size, alignment);
    ALLOCATOR_POST_REALLOCATE_HOOK(allocator, memory, new_size, alignment, result);

    return result;
}

static inline void allocator_dealloc(Allocator allocator, Memory memory) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DEALLOCATE_HOOK(allocator, memory);
    table.deallocate((void*)data, memory);
    ALLOCATOR_POST_DEALLOCATE_HOOK(allocator, memory);
}

static inline void allocator_destroy(Allocator allocator) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DESTROY_HOOK(allocator);
    table.destroy((void*)data);
    ALLOCATOR_POST_DESTROY_HOOK(allocator);
}


#define allocate(allocator, size, alignment) allocator_alloc(allocator, size, alignment)
#define reallocate(allocator, memory, new_size, alignment) allocator_realloc(allocator, memory, new_size, alignment)
#define deallocate(allocator, memory) allocator_dealloc(allocator, memory)
#define destroy_allocator(allocator) allocator_destroy(allocator)

#else
/* User options */
#ifndef ALLOCATOR_PRE_ALLOCATE_HOOK
#define ALLOCATOR_PRE_ALLOCATE_HOOK(allocator, size, alignment, file, function, line) ((void)allocator, (void)size, (void)alignment, (void)file, (void)function, (void)line)
#endif
#ifndef ALLOCATOR_POST_ALLOCATE_HOOK
#define ALLOCATOR_POST_ALLOCATE_HOOK(allocator, size, alignment, result, file, function, line) ((void)allocator, (void)size, (void)alignment, (void)result, (void)file, (void)function, (void)line)
#endif

#ifndef ALLOCATOR_PRE_REALLOCATE_HOOK
#define ALLOCATOR_PRE_REALLOCATE_HOOK(allocator, memory, new_size, alignment, file, function, line) ((void)allocator, (void)memory, (void)new_size, (void)alignment, (void)file, (void)function, (void)line)
#endif
#ifndef ALLOCATOR_POST_REALLOCATE_HOOK
#define ALLOCATOR_POST_REALLOCATE_HOOK(allocator, memory, new_size, alignment, result, file, function, line) ((void)allocator, (void)memory, (void)new_size, (void)alignment, (void)result, (void)file, (void)function, (void)line)
#endif

#ifndef ALLOCATOR_PRE_DEALLOCATE_HOOK
#define ALLOCATOR_PRE_DEALLOCATE_HOOK(allocator, memory, file, function, line) ((void)allocator, (void)memory, (void)file, (void)function, (void)line)
#endif
#ifndef ALLOCATOR_POST_DEALLOCATE_HOOK
#define ALLOCATOR_POST_DEALLOCATE_HOOK(allocator, memory, file, function, line) ((void)allocator, (void)memory, (void)file, (void)function, (void)line)
#endif

#ifndef ALLOCATOR_PRE_DESTROY_HOOK
#define ALLOCATOR_PRE_DESTROY_HOOK(allocator, file, function, line) ((void)allocator, (void)file, (void)function, (void)line)
#endif
#ifndef ALLOCATOR_POST_DESTROY_HOOK
#define ALLOCATOR_POST_DESTROY_HOOK(allocator, file, function, line) ((void)allocator, (void)file, (void)function, (void)line)
#endif


static inline Memory allocator_alloc(Allocator allocator, size_t size, size_t alignment, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_ALLOCATE_HOOK(allocator, size, alignment, file, function, line);
    Memory result = table.allocate((void*)data, size, alignment);
    ALLOCATOR_POST_ALLOCATE_HOOK(allocator, size, alignment, result, file, function, line);

    return result;
}

static inline Memory allocator_realloc(Allocator allocator, Memory memory, size_t new_size, size_t alignment, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_REALLOCATE_HOOK(allocator, memory, new_size, alignment, file, function, line);
    Memory result = table.reallocate((void*)data, memory, new_size, alignment);
    ALLOCATOR_POST_REALLOCATE_HOOK(allocator, memory, new_size, alignment, result, file, function, line);

    return result;
}

static inline void allocator_dealloc(Allocator allocator, Memory memory, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DEALLOCATE_HOOK(allocator, memory, file, function, line);
    table.deallocate((void*)data, memory);
    ALLOCATOR_POST_DEALLOCATE_HOOK(allocator, memory, file, function, line);
}

static inline void allocator_destroy(Allocator allocator, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DESTROY_HOOK(allocator, file, function, line);
    table.destroy((void*)data);
    ALLOCATOR_POST_DESTROY_HOOK(allocator, file, function, line);
}



#define allocate(allocator, size, alignment) allocator_alloc(allocator, size, alignment, __FILE_NAME__, __func__, __LINE__)
#define reallocate(allocator, old_memory, new_size, alignment) allocator_realloc(allocator, old_memory, new_size, alignment, __FILE_NAME__, __func__, __LINE__)
#define deallocate(allocator, memory) allocator_dealloc(allocator, memory, __FILE_NAME__, __func__, __LINE__)
#define destroy_allocator(allocator) allocator_destroy(allocator, __FILE_NAME__, __func__, __LINE__)

#endif











Memory allocate_system(Allocator allocator, size_t size, size_t alignment);
Memory reallocate_system(Allocator allocator, Memory memory, size_t new_size, size_t alignment);
void   deallocate_system(Allocator allocator, Memory memory);
void   destroy_system(Allocator allocator);






static inline int allocator_register_kind(AllocatorFunctionTable table) {
    if (allocator_function_kinds_count >= 16) {
        return -1;
    }

    allocator_function_kinds[allocator_function_kinds_count] = table;
    return allocator_function_kinds_count++;
}




#define allocator_to_base(allocator, kind) (Allocator)((uintptr_t)(allocator) | (kind));


#ifdef ALLOCATOR_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

inline Memory allocate_system(Allocator allocator, size_t size, size_t alignment) {
    (void)allocator;
    void* ptr = NULL;
    int result = posix_memalign(&ptr, alignment, size);
    if (result != 0) {
        return MEMORY_NULL;
    }
    return (Memory) { .base = ptr, .size = size };
}

inline Memory reallocate_system(Allocator allocator, Memory memory, size_t new_size, size_t alignment) {
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

inline void deallocate_system(Allocator allocator, Memory memory) {
    (void)allocator;
    free(memory.base);
}

inline void destroy_system(Allocator allocator) {
    (void)allocator;
}




#endif




#endif //ALLOCATORS_ALLOCATORS_H