#include "pool_allocator.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void panic(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static size_t pool_alignment(const struct PoolAllocator *pool) {
    const size_t alignment = pool->allocator.flag.alignment;
    return alignment >= ALLOCATOR_DEFAULT_ALIGNMENT ? alignment : ALLOCATOR_DEFAULT_ALIGNMENT;
}

static size_t pool_slot_stride(size_t slot_size, size_t alignment) {
    const size_t minimum_slot_size = slot_size >= sizeof(void *) ? slot_size : sizeof(void *);
    return align_up(minimum_slot_size, alignment);
}

static struct MemoryRegion pool_fail(const struct PoolAllocator *pool, const char *message) {
    switch (pool->allocator.flag.oom_strategy) {
        case OOM_STRATEGY_NULL:
            return (struct MemoryRegion){ .base = NULL, .size = 0 };
        case OOM_STRATEGY_PANIC:
        case OOM_STRATEGY_GROW:
        case OOM_STRATEGY_GROW_IF_POSSIBLE:
        default:
            panic(message);
    }

    return (struct MemoryRegion){ .base = NULL, .size = 0 };
}

static int pool_contains_pointer(const struct PoolAllocator *pool, const void *pointer) {
    const uintptr_t pool_base = (uintptr_t)pool->pool_start;
    const uintptr_t pool_limit = pool_base + (uintptr_t)(pool->slot_stride * pool->capacity);
    const uintptr_t candidate = (uintptr_t)pointer;

    if (candidate < pool_base || candidate >= pool_limit) {
        return 0;
    }

    return ((candidate - pool_base) % pool->slot_stride) == 0;
}

static int pool_initialize(struct PoolAllocator *pool) {
    const size_t alignment = pool_alignment(pool);
    const size_t raw_bytes = pool->slot_stride * pool->capacity;
    const size_t requested_bytes = raw_bytes + alignment - 1;
    struct MemoryRegion backing_region = allocate_from(pool->allocator.parent, requested_bytes);

    if (backing_region.base == NULL) {
        return -1;
    }

    const uintptr_t aligned_base = align_up((uintptr_t)backing_region.base, alignment);
    pool->backing_region = backing_region;
    pool->pool_start = (void *)aligned_base;
    pool->free_list = NULL;
    pool->free_count = 0;

    for (uint32_t index = 0; index < pool->capacity; ++index) {
        void *slot = (void *)(aligned_base + ((uintptr_t)index * pool->slot_stride));
        *(void **)slot = pool->free_list;
        pool->free_list = slot;
        pool->free_count += 1;
    }

    return 0;
}

static struct MemoryRegion pool_allocate(struct Allocator *allocator, size_t size) {
    struct PoolAllocator *pool = (struct PoolAllocator *)allocator;

    if (size > pool->slot_size) {
        return pool_fail(pool, "PoolAllocator: Requested size exceeds slot size!");
    }
    if (pool->capacity == 0 || pool->slot_size == 0) {
        return pool_fail(pool, "PoolAllocator: Invalid pool configuration!");
    }
    if (pool->pool_start == NULL && pool_initialize(pool) != 0) {
        return pool_fail(pool, "PoolAllocator: Failed to initialize backing storage!");
    }
    if (pool->free_list == NULL) {
        return pool_fail(pool, "PoolAllocator: Pool exhausted!");
    }

    void *slot = pool->free_list;
    pool->free_list = *(void **)slot;
    pool->free_count -= 1;
    memset(slot, 0, pool->slot_size);

    return (struct MemoryRegion){ .base = slot, .size = pool->slot_size };
}

static void pool_deallocate(struct Allocator *allocator, struct MemoryRegion region) {
    struct PoolAllocator *pool = (struct PoolAllocator *)allocator;

    assert(region.base != NULL);
    assert(pool->pool_start != NULL);
    assert(pool_contains_pointer(pool, region.base));

    *(void **)region.base = pool->free_list;
    pool->free_list = region.base;
    pool->free_count += 1;
}

static void pool_destroy(struct Allocator *allocator) {
    struct PoolAllocator *pool = (struct PoolAllocator *)allocator;

    if (pool->backing_region.base != NULL) {
        deallocate_from(pool->allocator.parent, pool->backing_region);
    }

    pool->backing_region = (struct MemoryRegion){ .base = NULL, .size = 0 };
    pool->pool_start = NULL;
    pool->free_list = NULL;
    pool->free_count = 0;
}

struct PoolAllocator make_pool_allocator(struct PoolAllocatorOptions options) {
    const uint8_t oom_strategy = options.allocator_options.oom_strategy != 0
            ? options.allocator_options.oom_strategy
            : OOM_STRATEGY_PANIC;
    const uint32_t alignment = options.allocator_options.alignment != 0
            ? options.allocator_options.alignment
            : (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT;
    const size_t effective_alignment = alignment >= ALLOCATOR_DEFAULT_ALIGNMENT
            ? alignment
            : ALLOCATOR_DEFAULT_ALIGNMENT;
    const size_t slot_stride = pool_slot_stride(options.slot_size, effective_alignment);

    assert(options.slot_size > 0);
    assert(options.capacity > 0);

    return (struct PoolAllocator){
        .allocator = {
            .allocate = pool_allocate,
            .reallocate = NULL,
            .deallocate = pool_deallocate,
            .destroy = pool_destroy,
            .parent = options.allocator_options.parent,
            .flag = {
                .oom_strategy = oom_strategy,
                .is_thread_safe = 0,
                .alignment = (uint32_t)effective_alignment,
                .size = (uint32_t)(sizeof(struct PoolAllocator) - sizeof(struct Allocator))
            },
        },
        .backing_region = { .base = NULL, .size = 0 },
        .pool_start = NULL,
        .free_list = NULL,
        .slot_size = options.slot_size,
        .slot_stride = slot_stride,
        .capacity = options.capacity,
        .free_count = 0,
    };
}

