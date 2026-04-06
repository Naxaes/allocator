#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "preamble.h"

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
typedef struct MemoryRegion (*AllocateFn)(struct Allocator *, size_t);
typedef struct MemoryRegion (*ReallocateFn)(struct Allocator *, struct MemoryRegion, size_t);
typedef void (*DeallocateFn)(struct Allocator *, struct MemoryRegion);
typedef void (*DestroyFn)(struct Allocator *);

struct AllocatorHandle {
    uint32_t id;
};

struct Allocator {
    AllocateFn allocate;
    ReallocateFn reallocate;
    DeallocateFn deallocate;
    DestroyFn destroy;
    const char* name;
    struct AllocatorHandle parent;
    struct AllocatorHandle previous;
    struct {
        uint32_t oom_strategy : 3,
                 supports_reallocation : 1,
                 supports_deallocation : 1,
                 is_thread_safe : 1,
                 alignment: 16,
                 reserved : 10;
    } flag;
    uint32_t size;
};

struct AllocatorOptions {
    const char* name;
    struct AllocatorHandle parent;
    uint8_t oom_strategy;
    uint32_t alignment;
};

extern const struct AllocatorHandle MAIN_ALLOCATOR_HANDLE;

struct AllocatorHandle push_allocator(const struct Allocator *allocator);
struct Allocator* get_current_allocator(void);
struct Allocator* get_allocator(struct AllocatorHandle handle);
void pop_allocator(void);
void allocator_cleanup(void);

struct MemoryRegion allocate_from(struct AllocatorHandle handle, size_t size);
struct MemoryRegion reallocate_from(struct AllocatorHandle handle, struct MemoryRegion region, size_t new_size);
void deallocate_from(struct AllocatorHandle handle, struct MemoryRegion region);

struct MemoryRegion allocate(size_t size);
struct MemoryRegion reallocate(struct MemoryRegion region, size_t new_size);
void deallocate(struct MemoryRegion region);


// ---- Helpers ----
static inline size_t align_up(size_t offset, size_t alignment) {
    const size_t remainder = offset % alignment;
    return remainder ? offset + (alignment - remainder) : offset;
}

static inline size_t align_down(size_t offset, size_t alignment) {
    return offset - (offset % alignment);
}

#define total_size_of_allocator(allocator) (sizeof(struct Allocator) + allocator->size)



#endif

