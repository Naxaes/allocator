#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#include "allocator_base.h"

#include <stdint.h>


struct AllocatorFunctionTable {
    AllocateFn   allocate;
    ReallocateFn reallocate;
    DeallocateFn deallocate;
    DestroyFn    destroy;
    QueryFn      query;
} allocator_function_kinds[ALLOCATOR_KINDS_COUNT];
static int allocator_function_kinds_count = 0;

typedef struct AllocatorFunctionTable AllocatorFunctionTable;


Allocator* allocator_current(void);
int allocator_push(Allocator* allocator);
void allocator_pop(void);
Allocator* allocator_get(size_t index);
int allocator_register_kind(AllocatorFunctionTable table);

#define with_allocator(allocator) for (int _allocator_with_it = allocator_push(allocator); _allocator_with_it; allocator_pop(), _allocator_with_it = 0)



#if ALLOCATOR_DEBUG
static inline Memory allocator_alloc(Allocator* allocator, size_t size, size_t alignment);
static inline Memory allocator_realloc(Allocator* allocator, Memory memory, size_t new_size, size_t alignment);
static inline void allocator_dealloc(Allocator* allocator, Memory memory);
static inline void allocator_destroy(Allocator* allocator);

#define allocate3(allocator, size, alignment) allocator_alloc(allocator, size, alignment)
#define reallocate4(allocator, old_memory, new_size, alignment) allocator_realloc(allocator, old_memory, new_size, alignment)
#define deallocate2(allocator, memory) allocator_dealloc(allocator, memory)
#define destroy_allocator1(allocator) allocator_destroy(allocator)

#define allocate2(size, alignment) allocator_alloc(allocator_current(), size, alignment)
#define reallocate3(old_memory, new_size, alignment) allocator_realloc(allocator_current(), old_memory, new_size, alignment)
#define deallocate1(memory) allocator_dealloc(allocator_current(), memory)
#define destroy_allocator0() allocator_destroy(allocator_current())

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

#define allocate(allocator, size, alignment) allocator_alloc(allocator, size, alignment)
#define reallocate(allocator, memory, new_size, alignment) allocator_realloc(allocator, memory, new_size, alignment)
#define deallocate(allocator, memory) allocator_dealloc(allocator, memory)
#define destroy_allocator(allocator) allocator_destroy(allocator)

static inline Memory allocator_alloc(Allocator* allocator, size_t size, size_t alignment) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_ALLOCATE_HOOK(allocator, size, alignment);
    Memory result = (table.allocate)((void*)data, size, alignment);
    ALLOCATOR_POST_ALLOCATE_HOOK(allocator, size, alignment, result);

    return result;
}

static inline Memory allocator_realloc(Allocator* allocator, Memory memory, size_t new_size, size_t alignment) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_REALLOCATE_HOOK(allocator, memory, new_size, alignment);
    Memory result = table.reallocate((void*)data, memory, new_size, alignment);
    ALLOCATOR_POST_REALLOCATE_HOOK(allocator, memory, new_size, alignment, result);

    return result;
}

static inline void allocator_dealloc(Allocator* allocator, Memory memory) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DEALLOCATE_HOOK(allocator, memory);
    table.deallocate((void*)data, memory);
    ALLOCATOR_POST_DEALLOCATE_HOOK(allocator, memory);
}

static inline void allocator_destroy(Allocator* allocator) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DESTROY_HOOK(allocator);
    table.destroy((void*)data);
    ALLOCATOR_POST_DESTROY_HOOK(allocator);
}

#else
static inline Memory allocator_alloc(Allocator* allocator, size_t size, size_t alignment, const char* file, const char* function, int line);
static inline Memory allocator_realloc(Allocator* allocator, Memory memory, size_t new_size, size_t alignment, const char* file, const char* function, int line);
static inline void allocator_dealloc(Allocator* allocator, Memory memory, const char* file, const char* function, int line);
static inline void allocator_destroy(Allocator* allocator, const char* file, const char* function, int line);

#define allocate3(allocator, size, alignment) allocator_alloc(allocator, size, alignment, __FILE_NAME__, __func__, __LINE__)
#define reallocate4(allocator, old_memory, new_size, alignment) allocator_realloc(allocator, old_memory, new_size, alignment, __FILE_NAME__, __func__, __LINE__)
#define deallocate2(allocator, memory) allocator_dealloc(allocator, memory, __FILE_NAME__, __func__, __LINE__)
#define destroy_allocator1(allocator) allocator_destroy(allocator, __FILE_NAME__, __func__, __LINE__)
#define query_allocator2(allocator, tag) allocator_query(allocator, tag)

#define allocate2(size, alignment) allocator_alloc(allocator_current(), size, alignment, __FILE_NAME__, __func__, __LINE__)
#define reallocate3(old_memory, new_size, alignment) allocator_realloc(allocator_current(), old_memory, new_size, alignment, __FILE_NAME__, __func__, __LINE__)
#define deallocate1(memory) allocator_dealloc(allocator_current(), memory, __FILE_NAME__, __func__, __LINE__)
#define destroy_allocator0() allocator_destroy(allocator_current(), __FILE_NAME__, __func__, __LINE__)
#define query_allocator1(tag) allocator_query(allocator_current(), tag)


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

static inline Memory allocator_alloc(Allocator* allocator, size_t size, size_t alignment, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_ALLOCATE_HOOK(allocator, size, alignment, file, function, line);
    Memory result = (table.allocate)((void*)data, size, alignment);
    ALLOCATOR_POST_ALLOCATE_HOOK(allocator, size, alignment, result, file, function, line);

    return result;
}

static inline Memory allocator_realloc(Allocator* allocator, Memory memory, size_t new_size, size_t alignment, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_REALLOCATE_HOOK(allocator, memory, new_size, alignment, file, function, line);
    Memory result = (table.reallocate)((void*)data, memory, new_size, alignment);
    ALLOCATOR_POST_REALLOCATE_HOOK(allocator, memory, new_size, alignment, result, file, function, line);

    return result;
}

static inline void allocator_dealloc(Allocator* allocator, Memory memory, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DEALLOCATE_HOOK(allocator, memory, file, function, line);
    (table.deallocate)((void*)data, memory);
    ALLOCATOR_POST_DEALLOCATE_HOOK(allocator, memory, file, function, line);
}

static inline void allocator_destroy(Allocator* allocator, const char* file, const char* function, int line) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    ALLOCATOR_PRE_DESTROY_HOOK(allocator, file, function, line);
    (table.destroy)((void*)data);
    ALLOCATOR_POST_DESTROY_HOOK(allocator, file, function, line);
}

static inline size_t allocator_query(Allocator* allocator, AllocatorQuery tag) {
    uint8_t   kind = (uintptr_t)allocator & ALLOCATOR_KIND_TAG_MASK;
    uintptr_t data = (uintptr_t)allocator & ALLOCATOR_KIND_DATA_MASK;
    AllocatorFunctionTable table = allocator_function_kinds[kind];

    if (table.query != NULL) {
        return table.query((void*)data, tag);
    } else {
        return (size_t)-1;
    }
}


#endif  // ALLOCATOR_DEBUG





/* ---- DEFAULT ARGUMENTS ----
Wrap the function in a variadic macro that calls to WITH_DEFAULTS with a
name and __VA_ARGS__. For each parameter passed in it'll now dispatch
to macros (or functions) named "nameX", where X is the number of parameters.

    // Define the dispatcher.
    #define greet(...) WITH_DEFAULTS(greet, __VA_ARGS__)

    // Define the overloaded functions (or function-like macros) you want.
    #define greet1(name)           printf("%s %s!", "Hello", name)
    #define greet2(greeting, name) printf("%s %s!", greeting, name)

    // Call.
    greet("Sailor");                      // printf("%s %s!", "Hello",     "Sailor");
    greet("Greetings", "Sailor");         // printf("%s %s!", "Greetings", "Sailor");
    greet("Greetings", "Sailor", "!!!");  // Error: greet3 is not defined.

This is restricted to a minimum of 1 argument and a maximum of 8.
*/
#define Z_CONCATENATE(arg1, arg2) arg1##arg2
#define Z_POP_10TH_ARG(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, argN, ...) argN
#define Z_VA_ARGS_COUNT(...)  Z_POP_10TH_ARG(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define Z_SELECT_FUNCTION(function, postfix) Z_CONCATENATE(function, postfix)
#define Z_WITH_DEFAULTS(f, ...) Z_SELECT_FUNCTION(f, Z_VA_ARGS_COUNT(__VA_ARGS__))(__VA_ARGS__)

#define allocate(...)          Z_WITH_DEFAULTS(allocate, __VA_ARGS__)
#define reallocate(...)        Z_WITH_DEFAULTS(reallocate, __VA_ARGS__)
#define deallocate(...)        Z_WITH_DEFAULTS(deallocate, __VA_ARGS__)
#define destroy_allocator(...) Z_WITH_DEFAULTS(destroy_allocator, __VA_ARGS__)
#define query_allocator(...)   Z_WITH_DEFAULTS(query_allocator, __VA_ARGS__)


#endif  // ALLOCATORS_H


















#ifdef ALLOCATORS_IMPLEMENTATION
#include <assert.h>


int allocator_register_kind(AllocatorFunctionTable table) {
    if (allocator_function_kinds_count >= ALLOCATOR_KINDS_COUNT) {
        return -1;
    }

    allocator_function_kinds[allocator_function_kinds_count] = table;
    return allocator_function_kinds_count++;
}



static Allocator** allocators = NULL;
static size_t allocators_count = 0;
static size_t allocators_capacity = 0;



int allocator_push(Allocator* allocator) {
    if (allocators_count < allocators_capacity) {
        assert(allocators_count < allocators_capacity);
        assert(allocators != NULL);
        allocators[allocators_count++] = allocator;
        return 1;
    }
    if (allocators == NULL) {
        Memory result = allocate(0, 8 * sizeof(Allocator*), ALLOCATOR_KINDS_COUNT);
        if (result.base == NULL) {
            return 0;
        }
        allocators = result.base;
        allocators_capacity = result.size / sizeof(Allocator*);
    } else {
        size_t new_capacity = allocators_capacity * 2;
        Memory old_memory = { .base = allocators, .size = allocators_capacity * sizeof(Allocator*) };
        Memory result = reallocate(0, old_memory, new_capacity * sizeof(Allocator*), ALLOCATOR_KINDS_COUNT);
        if (result.base == NULL) {
            return 0;
        }
        allocators = result.base;
        allocators_capacity = result.size / sizeof(Allocator*);
    }
    allocators[allocators_count++] = allocator;
    return 1;
}

void allocator_pop(void) {
    if (allocators_count > 0) {
        allocators[--allocators_count] = NULL;
    }
}

Allocator* allocator_current(void) {
    if (allocators_count < allocators_capacity) {
        return allocators[allocators_count - 1];
    } else {
        return NULL;
    }
}

Allocator* allocator_get(size_t index) {
    if (index < allocators_count) {
        return allocators[index];
    } else {
        return NULL;
    }
}


#endif // ALLOCATORS_IMPLEMENTATION

