#include "test_support.h"

#include <stdio.h>

#include "pool_allocator.h"
#include "slab_allocator.h"
#include "stack_allocator.h"
#include "system_allocator.h"

static struct AllocatorOptions default_system_options(void) {
    return (struct AllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    };
}

static int test_system_allocator_api(void) {
    const struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);

    TEST_CHECK(get_allocator(system_handle)->name != NULL);

    struct MemoryRegion region = allocate_from(system_handle, 64);
    TEST_CHECK(region.base != NULL);
    TEST_CHECK(test_region_is_aligned(region, ALLOCATOR_DEFAULT_ALIGNMENT));

    region = reallocate_from(system_handle, region, 160);
    TEST_CHECK(region.base != NULL);
    TEST_CHECK(region.size == 160);
    TEST_CHECK(test_region_is_aligned(region, ALLOCATOR_DEFAULT_ALIGNMENT));

    deallocate_from(system_handle, region);
    allocator_cleanup();
    return 0;
}

static int test_system_allocator_multiple_regions(void) {
    const struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    const size_t sizes[] = { 1, 3, 5, 64, 127 };
    struct MemoryRegion regions[sizeof(sizes) / sizeof(sizes[0])];

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        regions[i] = allocate_from(system_handle, sizes[i]);
        TEST_CHECK(regions[i].base != NULL);
        TEST_CHECK(regions[i].size == sizes[i]);
        TEST_CHECK(test_region_is_aligned(regions[i], ALLOCATOR_DEFAULT_ALIGNMENT));
    }

    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i) {
        deallocate_from(system_handle, regions[i]);
    }

    allocator_cleanup();
    return 0;
}

static int test_stack_allocator_alignment_and_growth(void) {
    const struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
        .parent = system_handle,
        .oom_strategy = OOM_STRATEGY_GROW,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    struct Allocator *allocator = &stack_allocator.allocator;
    const struct MemoryRegion region = allocator->allocate(allocator, 256);

    TEST_CHECK(region.base != NULL);
    TEST_CHECK(region.size == 256);
    TEST_CHECK(test_region_is_aligned(region, allocator->flag.alignment));
    TEST_CHECK(stack_allocator.pool != NULL);
    TEST_CHECK(stack_allocator.pool_size >= stack_allocator.used);

    allocator->destroy(allocator);
    allocator_cleanup();
    return 0;
}

static int test_stack_allocator_null_oom_path(void) {
    const struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
        .parent = system_handle,
        .oom_strategy = OOM_STRATEGY_NULL,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    struct Allocator *allocator = &stack_allocator.allocator;

    stack_allocator.pool = (uint8_t *)1;
    stack_allocator.pool_size = 0;
    stack_allocator.used = 0;

    {
        const struct MemoryRegion region = allocator->allocate(allocator, 64);
        TEST_CHECK(region.base == NULL);
        TEST_CHECK(region.size == 0);
    }

    stack_allocator.pool = NULL;
    allocator_cleanup();
    return 0;
}

static int test_pool_allocator_reuse_and_exhaustion(void) {
    const struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    const struct PoolAllocator pool_allocator = make_pool_allocator((struct PoolAllocatorOptions){
        .allocator_options = {
            .parent = system_handle,
            .oom_strategy = OOM_STRATEGY_NULL,
            .alignment = 32,
        },
        .slot_size = 24,
        .capacity = 4,
    });
    const struct AllocatorHandle pool_handle = push_allocator(&pool_allocator.allocator);
    struct MemoryRegion regions[4];

    for (size_t i = 0; i < 4; ++i) {
        regions[i] = allocate_from(pool_handle, 16);
        TEST_CHECK(regions[i].base != NULL);
        TEST_CHECK(regions[i].size == 24);
        TEST_CHECK(test_region_is_aligned(regions[i], 32));
    }

    {
        const struct MemoryRegion exhausted = allocate_from(pool_handle, 16);
        TEST_CHECK(exhausted.base == NULL);
        TEST_CHECK(exhausted.size == 0);
    }

    deallocate_from(pool_handle, regions[1]);
    {
        const struct MemoryRegion reused = allocate_from(pool_handle, 16);
        TEST_CHECK(reused.base == regions[1].base);
        TEST_CHECK(reused.size == 24);
        TEST_CHECK(test_region_is_aligned(reused, 32));
        deallocate_from(pool_handle, reused);
    }

    deallocate_from(pool_handle, regions[0]);
    deallocate_from(pool_handle, regions[2]);
    deallocate_from(pool_handle, regions[3]);
    allocator_cleanup();
    return 0;
}

static int test_slab_allocator_growth_and_reuse(void) {
    const struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    const struct SlabAllocator slab_allocator = make_slab_allocator((struct SlabAllocatorOptions){
        .allocator_options = {
            .parent = system_handle,
            .oom_strategy = OOM_STRATEGY_GROW,
            .alignment = 32,
        },
        .slot_size = 48,
        .slots_per_slab = 4,
    });
    const struct AllocatorHandle slab_handle = push_allocator(&slab_allocator.allocator);
    struct MemoryRegion regions[10];

    for (size_t i = 0; i < 10; ++i) {
        regions[i] = allocate_from(slab_handle, 32);
        TEST_CHECK(regions[i].base != NULL);
        TEST_CHECK(regions[i].size == 48);
        TEST_CHECK(test_region_is_aligned(regions[i], 32));
    }

    TEST_CHECK(((struct SlabAllocator *)get_allocator(slab_handle))->slab_count >= 3);

    deallocate_from(slab_handle, regions[3]);
    deallocate_from(slab_handle, regions[7]);

    {
        const struct MemoryRegion reused_a = allocate_from(slab_handle, 32);
        const struct MemoryRegion reused_b = allocate_from(slab_handle, 32);
        TEST_CHECK(reused_a.base != NULL);
        TEST_CHECK(reused_b.base != NULL);
        TEST_CHECK(test_region_is_aligned(reused_a, 32));
        TEST_CHECK(test_region_is_aligned(reused_b, 32));
        deallocate_from(slab_handle, reused_a);
        deallocate_from(slab_handle, reused_b);
    }

    for (size_t i = 0; i < 10; ++i) {
        if (i == 3 || i == 7) {
            continue;
        }
        deallocate_from(slab_handle, regions[i]);
    }

    allocator_cleanup();
    return 0;
}

int main(void) {
    if (test_system_allocator_api() != 0) {
        return 1;
    }
    if (test_system_allocator_multiple_regions() != 0) {
        return 1;
    }
    if (test_stack_allocator_alignment_and_growth() != 0) {
        return 1;
    }
    if (test_stack_allocator_null_oom_path() != 0) {
        return 1;
    }
    if (test_pool_allocator_reuse_and_exhaustion() != 0) {
        return 1;
    }
    if (test_slab_allocator_growth_and_reuse() != 0) {
        return 1;
    }

    puts("allocator test suite passed");
    return 0;
}

