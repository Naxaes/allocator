#include <stdio.h>
#include <string.h>

#include "allocator.h"
#include "stack_allocator.h"
#include "system_allocator.h"

static int run_current_without_push(void) {
    (void)get_current_allocator();
    return 0;
}

static int run_zero_size_allocate(void) {
    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);

    (void)allocate_from(system_handle, 0);
    return 0;
}

static int run_stack_reallocate_unsupported(void) {
    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    const struct StackAllocator stack_allocator = make_stack_allocator(system_handle, OOM_STRATEGY_GROW);
    const struct AllocatorHandle stack_handle = push_allocator(&stack_allocator.allocator);
    const struct MemoryRegion region = allocate_from(stack_handle, 32);

    (void)reallocate_from(stack_handle, region, 64);
    return 0;
}

static int run_stack_deallocate_unsupported(void) {
    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT,
    });
    const struct AllocatorHandle system_handle = push_allocator(&system_allocator.allocator);
    const struct StackAllocator stack_allocator = make_stack_allocator(system_handle, OOM_STRATEGY_GROW);
    const struct AllocatorHandle stack_handle = push_allocator(&stack_allocator.allocator);
    const struct MemoryRegion region = allocate_from(stack_handle, 32);

    deallocate_from(stack_handle, region);
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

    fprintf(stderr, "unknown scenario: %s\n", argv[1]);
    return 2;
}

