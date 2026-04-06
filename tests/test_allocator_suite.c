#include "test_support.h"

#include <stdio.h>

#include "stack_allocator.h"
#include "system_allocator.h"

static int test_system_allocator_api(void) {
    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
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
    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
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
    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator(system_handle, OOM_STRATEGY_GROW);
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
    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator(system_handle, OOM_STRATEGY_NULL);
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

    puts("allocator test suite passed");
    return 0;
}

