#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <assert.h>

#include "preamble.h"

enum OOM_Strategy {
    OOM_STRATEGY_PANIC = 0,
    OOM_STRATEGY_NULL = 1,
    OOM_STRATEGY_GROW = 2,
    OOM_STRATEGY_GROW_IF_POSSIBLE = 3
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
                 id : 12;
    } flag;
};

struct AllocatorOptions {
    const char* name;
    struct Allocator* parent;
    uint8_t oom_strategy;
    uint32_t alignment; // Power-of-two exponent: 4 -> 16-byte alignment, 0 -> default alignment.
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
enum {
    ALLOCATOR_MAX_ALIGNMENT_EXPONENT = 31
};

static inline uint32_t allocator_alignment_exponent_from_bytes(size_t alignment) {
    uint32_t exponent = 0;
    size_t value = 1;

    assert(alignment > 0);
    while (value < alignment) {
        value <<= 1;
        exponent += 1;
    }

    assert(value == alignment && "Alignment must be a power of two.");
    assert(exponent <= ALLOCATOR_MAX_ALIGNMENT_EXPONENT && "Alignment exponent exceeds Allocator bitfield capacity.");
    return exponent;
}

static inline uint32_t allocator_default_alignment_exponent(void) {
    return allocator_alignment_exponent_from_bytes(ALLOCATOR_DEFAULT_ALIGNMENT);
}

static inline uint32_t allocator_normalize_alignment_exponent(uint32_t alignment_exponent) {
    const uint32_t effective_exponent = alignment_exponent != 0
            ? alignment_exponent
            : allocator_default_alignment_exponent();

    assert(effective_exponent <= ALLOCATOR_MAX_ALIGNMENT_EXPONENT && "Alignment exponent exceeds Allocator bitfield capacity.");
    return effective_exponent;
}

static inline size_t allocator_alignment_bytes_from_exponent(uint32_t alignment_exponent) {
    assert(alignment_exponent <= ALLOCATOR_MAX_ALIGNMENT_EXPONENT && "Alignment exponent exceeds Allocator bitfield capacity.");
    return ((size_t)1) << alignment_exponent;
}

static inline size_t allocator_alignment_bytes_for(const struct Allocator *allocator) {
    assert(allocator != NULL);
    return allocator_alignment_bytes_from_exponent(allocator->flag.alignment);
}

static inline size_t align_up(size_t offset, size_t alignment) {
    const size_t remainder = offset % alignment;
    return remainder ? offset + (alignment - remainder) : offset;
}

static inline size_t align_down(size_t offset, size_t alignment) {
    return offset - (offset % alignment);
}

#define total_size_of_allocator(allocator) (sizeof(struct Allocator) + allocator->size)



#endif

