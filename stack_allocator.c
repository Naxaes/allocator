#include "stack_allocator.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static size_t stack_allocation_alignment(const struct StackAllocator *stack) {
    return allocator_alignment_bytes_for(&stack->allocator);
}

static void panic(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static int stack_resize_pool(struct StackAllocator *stack, size_t required_size) {
    const size_t alignment = stack_allocation_alignment(stack);
    size_t new_size = (stack->pool_size == 0) ? 1 : stack->pool_size;

    while (new_size < required_size) {
        new_size *= 2;
    }

    {
        const size_t requested_size = new_size + alignment - 1;
        const struct MemoryRegion new_backing_region = allocate_from(stack->allocator.parent, requested_size);
        uint8_t *new_pool = NULL;

        if (new_backing_region.base == NULL) {
            return -1;
        }

        new_pool = (uint8_t *)align_up((uintptr_t)new_backing_region.base, alignment);
        if (stack->pool != NULL && stack->used > 0) {
            memcpy(new_pool, stack->pool, stack->used);
            deallocate_from(stack->allocator.parent, stack->backing_region);
        }

        stack->backing_region = new_backing_region;
        stack->pool = new_pool;
        stack->pool_size = new_size;
    }

    if (stack->pool == NULL) {
        return -1;
    }
    return 0;
}

static struct MemoryRegion stack_allocate(struct Allocator *allocator, size_t size) {
    struct StackAllocator *stack = (struct StackAllocator *)allocator;
    const size_t alignment = stack_allocation_alignment(stack);
    const size_t aligned_used = align_up(stack->used, alignment);
    const size_t required_size = aligned_used + size;

    if (required_size > stack->pool_size) {
        switch (stack->allocator.flag.oom_strategy) {
            case OOM_STRATEGY_PANIC:
            case OOM_STRATEGY_GROW:
            case OOM_STRATEGY_GROW_IF_POSSIBLE:
                if (stack_resize_pool(stack, required_size) != 0) {
                    if (stack->allocator.flag.oom_strategy == OOM_STRATEGY_GROW_IF_POSSIBLE) {
                        return (struct MemoryRegion){ .base = NULL, .size = 0 };
                    }
                    panic("StackAllocator: Failed to grow memory pool!");
                }
                break;
            case OOM_STRATEGY_NULL:
                return (struct MemoryRegion){ .base = NULL, .size = 0 };
            default:
                panic("StackAllocator: Invalid OOM strategy!");
        }
    }

    void *base = stack->pool + aligned_used;
    assert(((uintptr_t)base % alignment) == 0);
    stack->used = required_size;
    return (struct MemoryRegion){ .base = base, .size = size };
}

static void stack_allocator_destroy(struct Allocator *allocator) {
    struct StackAllocator *stack = (struct StackAllocator *)allocator;

    if (stack->backing_region.base == NULL) {
        return;
    }

    deallocate_from(stack->allocator.parent, stack->backing_region);
    stack->backing_region = (struct MemoryRegion){ .base = NULL, .size = 0 };
    stack->pool = NULL;
    stack->pool_size = 0;
    stack->used = 0;
}

struct StackAllocator make_stack_allocator(struct AllocatorOptions options) {
    const uint8_t oom_strategy = options.oom_strategy != 0
            ? options.oom_strategy
            : OOM_STRATEGY_PANIC;
    const uint32_t alignment = allocator_normalize_alignment_exponent(options.alignment);

    return (struct StackAllocator){
        .allocator = {
            .allocate = stack_allocate,
            .reallocate = NULL,
            .deallocate = NULL,
            .destroy = stack_allocator_destroy,
            .parent = options.parent ? options.parent : get_current_allocator(),
            .flag = {
                .oom_strategy = oom_strategy,
                .is_thread_safe = 0,
                .alignment = alignment,
                .size = (uint32_t)(sizeof(struct StackAllocator) - sizeof(struct Allocator))
            },
        },
        .backing_region = { .base = NULL, .size = 0 },
        .pool = NULL,
        .pool_size = 0,
        .used = 0
    };
}

