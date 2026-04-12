#include "slab_allocator.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SlabNode {
    struct SlabNode* next;
    struct MemoryRegion node_region;
    struct MemoryRegion backing_region;
    void*  slab_start;
    void*  free_list;
    uint32_t free_count;
};

static void panic(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static size_t slab_alignment(const struct SlabAllocator *allocator) {
    const size_t alignment = allocator->allocator.flag.alignment;
    return alignment >= ALLOCATOR_DEFAULT_ALIGNMENT ? alignment : ALLOCATOR_DEFAULT_ALIGNMENT;
}

static size_t slab_slot_stride(size_t slot_size, size_t alignment) {
    const size_t minimum_slot_size = slot_size >= sizeof(void *) ? slot_size : sizeof(void *);
    return align_up(minimum_slot_size, alignment);
}

static struct MemoryRegion slab_fail(const struct SlabAllocator *allocator, const char *message) {
    switch (allocator->allocator.flag.oom_strategy) {
        case OOM_STRATEGY_NULL:
        case OOM_STRATEGY_GROW_IF_POSSIBLE:
            return (struct MemoryRegion){ .base = NULL, .size = 0 };
        case OOM_STRATEGY_PANIC:
        case OOM_STRATEGY_GROW:
        default:
            panic(message);
    }

    return (struct MemoryRegion){ .base = NULL, .size = 0 };
}

static int slab_contains_pointer(const struct SlabAllocator *allocator, const struct SlabNode *slab, const void *pointer) {
    const uintptr_t slab_base = (uintptr_t)slab->slab_start;
    const uintptr_t slab_limit = slab_base + (uintptr_t)(allocator->slot_stride * allocator->slots_per_slab);
    const uintptr_t candidate = (uintptr_t)pointer;

    if (candidate < slab_base || candidate >= slab_limit) {
        return 0;
    }

    return ((candidate - slab_base) % allocator->slot_stride) == 0;
}

static int slab_allocate_node(struct SlabAllocator *allocator) {
    const size_t alignment = slab_alignment(allocator);
    const size_t slab_bytes = (allocator->slot_stride * allocator->slots_per_slab) + alignment - 1;
    struct MemoryRegion node_region = allocate_from(allocator->allocator.parent, sizeof(struct SlabNode));

    if (node_region.base == NULL) {
        return -1;
    }

    struct MemoryRegion backing_region = allocate_from(allocator->allocator.parent, slab_bytes);
    if (backing_region.base == NULL) {
        deallocate_from(allocator->allocator.parent, node_region);
        return -1;
    }

    struct SlabNode *slab = (struct SlabNode *)node_region.base;
    memset(slab, 0, sizeof(*slab));
    slab->node_region = node_region;
    slab->backing_region = backing_region;
    slab->next = allocator->slabs;
    slab->slab_start = (void *)align_up((uintptr_t)backing_region.base, alignment);
    slab->free_list = NULL;
    slab->free_count = 0;

    for (uint32_t index = 0; index < allocator->slots_per_slab; ++index) {
        void *slot = (void *)((uintptr_t)slab->slab_start + ((uintptr_t)index * allocator->slot_stride));
        *(void **)slot = slab->free_list;
        slab->free_list = slot;
        slab->free_count += 1;
    }

    allocator->slabs = slab;
    allocator->slab_count += 1;
    return 0;
}

static struct SlabNode *slab_find_with_free_space(struct SlabAllocator* allocator) {
    for (struct SlabNode* slab = allocator->slabs; slab != NULL; slab = slab->next) {
        if (slab->free_list != NULL) {
            return slab;
        }
    }

    return NULL;
}

static struct MemoryRegion slab_allocate(struct Allocator* allocator_base, size_t size) {
    struct SlabAllocator* allocator = (struct SlabAllocator *)allocator_base;

    if (size > allocator->slot_size) {
        return slab_fail(allocator, "SlabAllocator: Requested size exceeds slot size!");
    }
    if (allocator->slot_size == 0 || allocator->slots_per_slab == 0) {
        return slab_fail(allocator, "SlabAllocator: Invalid slab configuration!");
    }

    struct SlabNode *slab = slab_find_with_free_space(allocator);
    if (slab == NULL) {
        if (slab_allocate_node(allocator) != 0) {
            return slab_fail(allocator, "SlabAllocator: Failed to grow slabs!");
        }
        slab = allocator->slabs;
    }

    void* slot = slab->free_list;
    slab->free_list = *(void **)slot;
    slab->free_count -= 1;
    memset(slot, 0, allocator->slot_size);

    return (struct MemoryRegion){ .base = slot, .size = allocator->slot_size };
}

static void slab_deallocate(struct Allocator* allocator_base, struct MemoryRegion region) {
    struct SlabAllocator* allocator = (struct SlabAllocator *)allocator_base;

    assert(region.base != NULL);

    for (struct SlabNode* slab = allocator->slabs; slab != NULL; slab = slab->next) {
        if (!slab_contains_pointer(allocator, slab, region.base)) {
            continue;
        }

        *(void **)region.base = slab->free_list;
        slab->free_list = region.base;
        slab->free_count += 1;
        return;
    }

    assert(!"SlabAllocator: deallocated pointer does not belong to any slab");
}

static void slab_destroy(struct Allocator* allocator_base) {
    struct SlabAllocator* allocator = (struct SlabAllocator*)allocator_base;
    struct SlabNode* slab = allocator->slabs;

    while (slab != NULL) {
        struct SlabNode *next = slab->next;
        deallocate_from(allocator->allocator.parent, slab->backing_region);
        deallocate_from(allocator->allocator.parent, slab->node_region);
        slab = next;
    }

    allocator->slabs = NULL;
    allocator->slab_count = 0;
}

struct SlabAllocator make_slab_allocator(struct SlabAllocatorOptions options) {
    const uint8_t oom_strategy = options.allocator_options.oom_strategy != 0
            ? options.allocator_options.oom_strategy
            : OOM_STRATEGY_GROW;
    const uint32_t alignment = options.allocator_options.alignment != 0
            ? options.allocator_options.alignment
            : (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT;
    const size_t effective_alignment = alignment >= ALLOCATOR_DEFAULT_ALIGNMENT
            ? alignment
            : ALLOCATOR_DEFAULT_ALIGNMENT;
    const size_t slot_stride = slab_slot_stride(options.slot_size, effective_alignment);

    assert(options.slot_size > 0);
    assert(options.slots_per_slab > 0);

    return (struct SlabAllocator){
        .allocator = {
            .allocate = slab_allocate,
            .reallocate = NULL,
            .deallocate = slab_deallocate,
            .destroy = slab_destroy,
            .parent = options.allocator_options.parent ? options.allocator_options.parent : get_current_allocator(),
            .flag = {
                .oom_strategy = oom_strategy,
                .is_thread_safe = 0,
                .alignment = (uint32_t)effective_alignment,
                .size = (uint32_t)(sizeof(struct SlabAllocator) - sizeof(struct Allocator))
            },
        },
        .slabs = NULL,
        .slot_size = options.slot_size,
        .slot_stride = slot_stride,
        .slots_per_slab = options.slots_per_slab,
        .slab_count = 0,
    };
}

