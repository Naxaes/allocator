#include <stdio.h>
#include <string.h>

#include "allocator.h"
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

static int run_current_without_push(void) {
    (void)allocate(16);
    return 0;
}

static int run_zero_size_allocate(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);

    (void)allocate_from(&system_allocator.allocator, 0);
    return 0;
}

static int run_stack_reallocate_unsupported(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
        .parent = &system_allocator.allocator,
        .oom_strategy = OOM_STRATEGY_GROW,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    push_allocator(&stack_allocator.allocator);
    const struct MemoryRegion region = allocate(32);

    (void)reallocate(region, 64);
    return 0;
}

static int run_stack_deallocate_unsupported(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
        .parent = &system_allocator.allocator,
        .oom_strategy = OOM_STRATEGY_GROW,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    push_allocator(&stack_allocator.allocator);
    const struct MemoryRegion region = allocate(32);

    deallocate(region);
    return 0;
}

static int run_pool_reallocate_unsupported(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct PoolAllocator pool_allocator = make_pool_allocator((struct PoolAllocatorOptions){
        .allocator_options = {
            .parent = &system_allocator.allocator,
            .oom_strategy = OOM_STRATEGY_PANIC,
            .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
        },
        .slot_size = 32,
        .capacity = 2,
    });
    push_allocator(&pool_allocator.allocator);
    const struct MemoryRegion region = allocate(16);

    (void)reallocate(region, 24);
    return 0;
}

static int run_slab_reallocate_unsupported(void) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct SlabAllocator slab_allocator = make_slab_allocator((struct SlabAllocatorOptions){
        .allocator_options = {
            .parent = &system_allocator.allocator,
            .oom_strategy = OOM_STRATEGY_GROW,
            .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
        },
        .slot_size = 32,
        .slots_per_slab = 2,
    });
    push_allocator(&slab_allocator.allocator);
    const struct MemoryRegion region = allocate(16);

    (void)reallocate(region, 24);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <scenario>\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "current_without_push") == 0) {
        return run_current_without_push();
    }
    if (strcmp(argv[1], "zero_size_allocate") == 0) {
        return run_zero_size_allocate();
    }
    if (strcmp(argv[1], "stack_reallocate_unsupported") == 0) {
        return run_stack_reallocate_unsupported();
    }
    if (strcmp(argv[1], "stack_deallocate_unsupported") == 0) {
        return run_stack_deallocate_unsupported();
    }
    if (strcmp(argv[1], "pool_reallocate_unsupported") == 0) {
        return run_pool_reallocate_unsupported();
    }
    if (strcmp(argv[1], "slab_reallocate_unsupported") == 0) {
        return run_slab_reallocate_unsupported();
    }

    fprintf(stderr, "unknown scenario: %s\n", argv[1]);
    return 2;
}

