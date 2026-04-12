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

struct Allocator {
    AllocateFn   allocate;      // Required
    ReallocateFn reallocate;    // Nullable
    DeallocateFn deallocate;    // Nullable
    DestroyFn    destroy;       // Nullable
    struct Allocator* parent;   // Nullable
    struct {
        uint32_t oom_strategy : 2,
                 is_thread_safe : 1,
                 alignment : 5,
                 size: 12,
                 id : 11;
    } flag;
};

struct AllocatorOptions {
    const char* name;
    struct Allocator* parent;
    uint8_t oom_strategy;
    uint32_t alignment;
};

void push_allocator(struct Allocator* allocator);
struct Allocator* get_current_allocator(void);
void pop_allocator(void);

struct MemoryRegion allocate_from(struct Allocator* allocator, size_t size);
struct MemoryRegion reallocate_from(struct Allocator* allocator, struct MemoryRegion region, size_t new_size);
void deallocate_from(struct Allocator* allocator, struct MemoryRegion region);

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

