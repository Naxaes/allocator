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
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);

    struct MemoryRegion region = allocate(64);
    TEST_CHECK(region.base != NULL);
    TEST_CHECK(test_region_is_aligned(region, ALLOCATOR_DEFAULT_ALIGNMENT));

    region = reallocate(region, 160);
    TEST_CHECK(region.base != NULL);
    TEST_CHECK(region.size == 160);
    TEST_CHECK(test_region_is_aligned(region, ALLOCATOR_DEFAULT_ALIGNMENT));

    deallocate(region);
    pop_allocator();
    return 0;
}

static int test_system_allocator_multiple_regions(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    const size_t sizes[] = { 1, 3, 5, 64, 127 };
    struct MemoryRegion regions[sizeof(sizes) / sizeof(sizes[0])];

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        regions[i] = allocate(sizes[i]);
        TEST_CHECK(regions[i].base != NULL);
        TEST_CHECK(regions[i].size == sizes[i]);
        TEST_CHECK(test_region_is_aligned(regions[i], ALLOCATOR_DEFAULT_ALIGNMENT));
    }

    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i) {
        deallocate(regions[i]);
    }

    pop_allocator();
    return 0;
}

static int test_stack_allocator_alignment_and_growth(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
        .parent = &system_allocator.allocator,
        .oom_strategy = OOM_STRATEGY_GROW,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    push_allocator(&stack_allocator.allocator);
    const struct MemoryRegion region = allocate(256);

    TEST_CHECK(region.base != NULL);
    TEST_CHECK(region.size == 256);
    TEST_CHECK(test_region_is_aligned(region, stack_allocator.allocator.flag.alignment));
    TEST_CHECK(stack_allocator.pool != NULL);
    TEST_CHECK(stack_allocator.pool_size >= stack_allocator.used);

    pop_allocator();
    pop_allocator();
    return 0;
}

static int test_stack_allocator_null_oom_path(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
        .parent = &system_allocator.allocator,
        .oom_strategy = OOM_STRATEGY_NULL,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });

    stack_allocator.pool = (uint8_t *)1;
    stack_allocator.pool_size = 0;
    stack_allocator.used = 0;

    {
        const struct MemoryRegion region = allocate_from(&stack_allocator.allocator, 64);
        TEST_CHECK(region.base == NULL);
        TEST_CHECK(region.size == 0);
    }

    stack_allocator.pool = NULL;
    pop_allocator();
    return 0;
}

static int test_pool_allocator_reuse_and_exhaustion(void) {
    const uint32_t alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT;
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct PoolAllocator pool_allocator = make_pool_allocator((struct PoolAllocatorOptions){
        .allocator_options = {
            .parent = &system_allocator.allocator,
            .oom_strategy = OOM_STRATEGY_NULL,
            .alignment = alignment,
        },
        .slot_size = 24,
        .capacity = 4,
    });
    push_allocator(&pool_allocator.allocator);
    struct MemoryRegion regions[4];

    for (size_t i = 0; i < 4; ++i) {
        regions[i] = allocate(16);
        TEST_CHECK(regions[i].base != NULL);
        TEST_CHECK(regions[i].size == 24);
        TEST_CHECK(test_region_is_aligned(regions[i], alignment));
    }

    {
        const struct MemoryRegion exhausted = allocate(16);
        TEST_CHECK(exhausted.base == NULL);
        TEST_CHECK(exhausted.size == 0);
    }

    deallocate(regions[1]);
    {
        const struct MemoryRegion reused = allocate(16);
        TEST_CHECK(reused.base == regions[1].base);
        TEST_CHECK(reused.size == 24);
        TEST_CHECK(test_region_is_aligned(reused, alignment));
        deallocate(reused);
    }

    deallocate(regions[0]);
    deallocate(regions[2]);
    deallocate(regions[3]);
    pop_allocator();
    pop_allocator();
    return 0;
}

static int test_slab_allocator_growth_and_reuse(void) {
    const uint32_t alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT;
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct SlabAllocator slab_allocator = make_slab_allocator((struct SlabAllocatorOptions){
        .allocator_options = {
            .parent = &system_allocator.allocator,
            .oom_strategy = OOM_STRATEGY_GROW,
            .alignment = alignment,
        },
        .slot_size = 48,
        .slots_per_slab = 4,
    });
    push_allocator(&slab_allocator.allocator);
    struct MemoryRegion regions[10];

    for (size_t i = 0; i < 10; ++i) {
        regions[i] = allocate(32);
        TEST_CHECK(regions[i].base != NULL);
        TEST_CHECK(regions[i].size == 48);
        TEST_CHECK(test_region_is_aligned(regions[i], alignment));
    }

    TEST_CHECK(slab_allocator.slab_count >= 3);

    deallocate(regions[3]);
    deallocate(regions[7]);

    {
        const struct MemoryRegion reused_a = allocate(32);
        const struct MemoryRegion reused_b = allocate(32);
        TEST_CHECK(reused_a.base != NULL);
        TEST_CHECK(reused_b.base != NULL);
        TEST_CHECK(test_region_is_aligned(reused_a, alignment));
        TEST_CHECK(test_region_is_aligned(reused_b, alignment));
        deallocate(reused_a);
        deallocate(reused_b);
    }

    for (size_t i = 0; i < 10; ++i) {
        if (i == 3 || i == 7) {
            continue;
        }
        deallocate(regions[i]);
    }

    pop_allocator();
    pop_allocator();
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

