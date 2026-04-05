#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define panic(msg) do { fprintf(stderr, "%s\n", msg); exit(EXIT_FAILURE); } while(0)



// ---------------- Base Allocator System ----------------
enum OOM_Strategy {
    OOM_STRATEGY_PANIC = 1,
    OOM_STRATEGY_NULL = 2,
    OOM_STRATEGY_GROW = 3,
    OOM_STRATEGY_GROW_IF_POSSIBLE = 4
};


struct MemoryRegion {
    void*  base;
    size_t size;
};

struct Allocator;
typedef struct MemoryRegion (*AllocateFn)(struct Allocator*, size_t);
typedef struct MemoryRegion (*ReallocateFn)(struct Allocator*, struct MemoryRegion, size_t);
typedef void (*DeallocateFn)(struct Allocator*, struct MemoryRegion);
typedef void (*DestroyFn)(struct Allocator*);

struct AllocatorHandle { uint32_t id; };

struct Allocator {
    AllocateFn   allocate;
    ReallocateFn reallocate;
    DeallocateFn deallocate;
    DestroyFn    destroy;
    const char*  name;
    struct AllocatorHandle parent;
    struct {
        uint32_t oom_strategy: 2,
                 supports_reallocation: 1,
                 supports_deallocation: 1,
                 is_thread_safe: 1,
                 reserved: 27;
    } flag;
    uint32_t size;
};

static const struct AllocatorHandle MAIN_ALLOCATOR_HANDLE = { .id = 0 };

static uint8_t* allocators;
static uint32_t allocator_offset = 0;
static uint32_t allocator_capacity = 0;
static struct AllocatorHandle allocator_current = MAIN_ALLOCATOR_HANDLE;

int allocators_grow(void) {
    uint32_t new_capacity = (allocator_capacity == 0) ? 64 : allocator_capacity * 2;
    uint8_t* new_allocators = realloc(allocators, new_capacity * sizeof(struct Allocator));
    if (new_allocators == NULL) {
        return -1; // Failed to grow
    }
    allocators = new_allocators;
    allocator_capacity = new_capacity;
    return 0; // Successfully grew
}

struct AllocatorHandle push_allocator(const struct Allocator* allocator) {
    if (allocator_offset >= allocator_capacity) {
        int result = allocators_grow();
        assert(result == 0);
    }

    uint32_t total_size = sizeof(struct Allocator) + allocator->size;

    const struct AllocatorHandle id = (struct AllocatorHandle){ .id = allocator_offset };
    memcpy(&allocators[allocator_offset], allocator, total_size);

    allocator_offset += total_size;
    allocator_current = id;
    return id;
}

struct Allocator* get_current_allocator(void) {
    const struct AllocatorHandle handle = allocator_current;

    assert(handle.id < allocator_offset);
    struct Allocator* allocator = (struct Allocator*)&allocators[handle.id];

    // Ensure that the current allocator is always the most recently pushed one
    assert(handle.id + sizeof(struct Allocator) + allocator->size == allocator_offset);
    return allocator;
}

struct Allocator* get_allocator(struct AllocatorHandle handle) {
    assert(handle.id <= allocator_offset);
    return (struct Allocator*)&allocators[handle.id];
}

void pop_allocator(struct AllocatorHandle handle) {
    assert(handle.id != 0);
    assert(handle.id == allocator_current.id); // Ensure we're popping the current allocator
    struct Allocator* allocator = get_allocator(handle);

    allocator->destroy(allocator);
    allocator_offset = handle.id; // Roll back to the previous allocator
    allocator_current = (struct AllocatorHandle){ .id = handle.id - allocator->size - sizeof(struct Allocator) }; // Set current to the previous allocator
}


/**
 * Allocate memory from a specific allocator. May panic if the oom strategy is set to panic on OOM.
 * @param handle The allocator to allocate from. Must be a valid allocator handle that was returned by push_allocator.
 * @param size The requested size of the memory to allocate. Can't be 0.
 * @return One of the following:
 *   - If the allocation succeeded, MemoryRegion with a valid base pointer and a size of at least the requested size (or greater).
 *   - If the allocation failed, MemoryRegion with a null base pointer and a size of 0.
 */
static inline struct MemoryRegion allocate_from(struct AllocatorHandle handle, size_t size) {
    struct Allocator* allocator = get_allocator(handle);
    return allocator->allocate(allocator, size);
}

/**
 * Reallocate memory from a specific allocator. May panic if the oom strategy is set to panic on OOM.
 * @param handle The allocator to reallocate from. Must be a valid allocator handle that was returned by push_allocator.
 * @param region The memory region to reallocate. Must have been allocated from the same allocator with correct base and size.
 * @param new_size The requested new size of the memory region. Can't be 0.
 * @return One of the following:
 *   - If the reallocation succeeded, MemoryRegion with a valid base pointer and a size of at least the requested new size (or greater).
 *   - If the reallocation failed, MemoryRegion with a null base pointer and a size of 0.
 */
static inline struct MemoryRegion reallocate_from(struct AllocatorHandle handle, struct MemoryRegion region, size_t new_size) {
    struct Allocator* allocator = get_allocator(handle);
    assert(allocator->flag.supports_reallocation && "Allocator doesn't support reallocation!");
    return allocator->reallocate(allocator, region, new_size);
}

/**
 * Deallocate memory from a specific allocator. May panic if the allocator doesn't support deallocation or if the region is invalid.
 * @param handle The allocator to deallocate from. Must be a valid allocator handle that was returned by push_allocator.
 * @param region The memory region to deallocate. Must have been allocated from the same allocator with correct base and size.
 */
static inline void deallocate_from(struct AllocatorHandle handle, struct MemoryRegion region) {
    struct Allocator* allocator = get_allocator(handle);
    assert(allocator->flag.supports_deallocation && "Allocator doesn't support deallocation!");
    allocator->deallocate(allocator, region);
}

static inline struct MemoryRegion allocate(size_t size) {
    return allocate_from(allocator_current, size);
}

static inline struct MemoryRegion reallocate(struct MemoryRegion region, size_t new_size) {
    return reallocate_from(allocator_current, region, new_size);
}

static inline void deallocate(struct MemoryRegion region) {
    deallocate_from(allocator_current, region);
}

// ---------------- System Allocator ----------------
struct SystemAllocator {
    struct Allocator allocator;
};

struct MemoryRegion system_allocate(struct Allocator* allocator, size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        switch (allocator->flag.oom_strategy) {
            case OOM_STRATEGY_PANIC:
                panic("SystemAllocator: Out of memory!\n");
            case OOM_STRATEGY_NULL:
                return (struct MemoryRegion){ .base = NULL, .size = 0 };
            default:
                panic("SystemAllocator: Invalid OOM strategy!\n");
        }
    }
    return (struct MemoryRegion){ .base = ptr, .size = size };
}

struct MemoryRegion system_reallocate(struct Allocator* allocator, struct MemoryRegion region, size_t new_size) {
    void* ptr = realloc(region.base, new_size);
    if (ptr == NULL) {
        switch (allocator->flag.oom_strategy) {
            case OOM_STRATEGY_PANIC:
                panic("SystemAllocator: Out of memory during reallocation!\n");
            case OOM_STRATEGY_NULL:
                return (struct MemoryRegion){ .base = NULL, .size = 0 };
            default:
                panic("SystemAllocator: Invalid OOM strategy!\n");
        }
    }
    return (struct MemoryRegion){ .base = ptr, .size = new_size };
}

void system_free(struct Allocator* allocator, struct MemoryRegion region) {
    (void)allocator; // Unused parameter
    free(region.base);
}

void system_destroy(struct Allocator* allocator) {
    (void)allocator; // Unused parameter
}

struct SystemAllocator make_system_allocator(void) {
    struct SystemAllocator allocator = {
        .allocator = {
            .allocate = system_allocate,
            .reallocate = system_reallocate,
            .deallocate = system_free,
            .destroy = system_destroy,
            .name = "SystemAllocator",
            .parent = MAIN_ALLOCATOR_HANDLE,
            .flag = {
                .oom_strategy = OOM_STRATEGY_PANIC,
                .supports_reallocation = 1,
                .supports_deallocation = 1
            },
            .size = sizeof(struct SystemAllocator) - sizeof(struct Allocator) // Size of the data after the Allocator struct
        }
    };
    return allocator;
}


// ---------------- Stack Allocator ----------------
struct StackAllocator {
    struct Allocator allocator;
    uint8_t*  pool;
    size_t    pool_size;
    size_t    used;
};

struct MemoryRegion stack_allocate(struct Allocator* allocator, size_t size) {
    struct StackAllocator* stack = (struct StackAllocator*)allocator;

    if (stack->used + size > stack->pool_size) {
        if (stack->pool == NULL) {
            struct MemoryRegion pool_region = allocate_from(stack->allocator.parent, size);
            if (pool_region.base != NULL) {
                stack->pool = pool_region.base;
                stack->pool_size = pool_region.size;
                void* base = stack->pool + stack->used;
                stack->used += size;
                return (struct MemoryRegion){ .base = base, .size = size };
            }
        }
    
        switch (stack->allocator.flag.oom_strategy) {
            case OOM_STRATEGY_PANIC:
                panic("StackAllocator: Out of memory!\n");
            case OOM_STRATEGY_NULL:
                return (struct MemoryRegion){ .base = NULL, .size = 0 };
            case OOM_STRATEGY_GROW:
            case OOM_STRATEGY_GROW_IF_POSSIBLE:
                size_t new_size = stack->pool_size * 2;
                while (stack->used + size > new_size) {
                    new_size *= 2;
                }
                struct MemoryRegion old_pool = { .base = stack->pool, .size = stack->pool_size };
                struct MemoryRegion new_pool = reallocate_from(stack->allocator.parent, old_pool, new_size);
                if (new_pool.base == NULL) {
                    if (stack->allocator.flag.oom_strategy == OOM_STRATEGY_GROW_IF_POSSIBLE) {
                        return (struct MemoryRegion){ .base = NULL, .size = 0 };
                    } else {
                        panic("StackAllocator: Failed to grow memory pool!\n");
                    }
                }
                stack->pool = new_pool.base;
                stack->pool_size = new_pool.size;
                break;
            default:
                panic("StackAllocator: Invalid OOM strategy!\n");
        }
    }

    void* base = stack->pool + stack->used;
    stack->used += size;
    return (struct MemoryRegion){ .base = base, .size = size };
}

void stack_allocator_destroy(struct Allocator* allocator) {
    struct StackAllocator* stack = (struct StackAllocator*)allocator;
    struct MemoryRegion pool_region = { .base = stack->pool, .size = stack->pool_size };
    deallocate_from(stack->allocator.parent, pool_region);
}


struct StackAllocator make_stack_allocator(struct AllocatorHandle parent, uint8_t oom_strategy) {
    struct StackAllocator stack = {
        .allocator = {
            .allocate = stack_allocate,
            .reallocate = NULL, // Stack allocator doesn't support reallocation
            .deallocate = NULL, // Stack allocator doesn't support deallocation
            .destroy = stack_allocator_destroy,
            .parent = parent,
            .name = "StackAllocator",
            .flag = {
                .oom_strategy = oom_strategy,
                .supports_reallocation = 0,
                .supports_deallocation = 0
            },
            .size = sizeof(struct StackAllocator) - sizeof(struct Allocator) // Size of the data after the Allocator struct
        },
        .pool = NULL,
        .pool_size = 0,
        .used = 0,
    };

    return stack;
}


int main(void) {
    struct SystemAllocator system_allocator = make_system_allocator();
    printf("System allocator created!\n");
    struct AllocatorHandle system_allocator_handle = push_allocator((struct Allocator*)&system_allocator);


    struct StackAllocator stack_allocator = make_stack_allocator(system_allocator_handle, OOM_STRATEGY_GROW);

    struct MemoryRegion mem = stack_allocate((struct Allocator*)&stack_allocator, 1024);
    printf("Allocated memory at %p with size %zu\n", mem.base, mem.size);

    struct MemoryRegion mem2 = stack_allocate((struct Allocator*)&stack_allocator, 2048);
    printf("Allocated memory at %p with size %zu\n", mem2.base, mem2.size);

    struct MemoryRegion mem3 = stack_allocate((struct Allocator*)&stack_allocator, 4096);
    printf("Allocated memory at %p with size %zu\n", mem3.base, mem3.size);

    struct AllocatorHandle stack_handle = push_allocator((struct Allocator*)&stack_allocator);
    {
        printf("Pushed stack allocator with handle id %u\n", stack_handle.id);

        struct MemoryRegion mem4 = allocate(512);
        printf("Allocated memory at %p with size %zu from current allocator\n", mem4.base, mem4.size);
    }
    stack_allocator_destroy((struct Allocator*)&stack_allocator);

    system_destroy((struct Allocator*)&system_allocator);
    return 0;
}



















