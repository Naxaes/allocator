#include "stack_allocator.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static size_t stack_allocation_alignment(const struct StackAllocator *stack) {
    size_t alignment = stack->allocator.flag.alignment;

    if (alignment == 0) {
        alignment = ALLOCATOR_DEFAULT_ALIGNMENT;
    }

    return alignment;
}

static size_t stack_align_up(size_t offset, size_t alignment) {
    const size_t remainder = offset % alignment;
    if (remainder == 0) {
        return offset;
    }

    return offset + (alignment - remainder);
}

static void panic(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static int stack_resize_pool(struct StackAllocator *stack, size_t required_size) {
    size_t new_size = (stack->pool_size == 0) ? 1 : stack->pool_size;
    while (new_size < required_size) {
        new_size *= 2;
    }

    if (stack->pool == NULL) {
        const struct MemoryRegion new_pool = allocate_from(stack->allocator.parent, new_size);
        if (new_pool.base == NULL) {
            return -1;
        }

        stack->pool = new_pool.base;
        stack->pool_size = new_pool.size;
        return 0;
    }

    const struct MemoryRegion old_pool = { .base = stack->pool, .size = stack->pool_size };
    const struct MemoryRegion new_pool = reallocate_from(stack->allocator.parent, old_pool, new_size);
    if (new_pool.base == NULL) {
        return -1;
    }

    stack->pool = new_pool.base;
    stack->pool_size = new_pool.size;
    return 0;
}

static struct MemoryRegion stack_allocate(struct Allocator *allocator, size_t size) {
    struct StackAllocator *stack = (struct StackAllocator *)allocator;
    const size_t alignment = stack_allocation_alignment(stack);
    const size_t aligned_used = stack_align_up(stack->used, alignment);
    const size_t required_size = aligned_used + size;

    if (required_size > stack->pool_size) {
        if (stack->pool == NULL) {
            const struct MemoryRegion initial_pool = allocate_from(stack->allocator.parent, required_size);
            if (initial_pool.base != NULL) {
                stack->pool = initial_pool.base;
                stack->pool_size = initial_pool.size;
            }
        }

        if (required_size > stack->pool_size) {
            switch (stack->allocator.flag.oom_strategy) {
                case OOM_STRATEGY_PANIC:
                    panic("StackAllocator: Out of memory!");
                case OOM_STRATEGY_NULL:
                    return (struct MemoryRegion){ .base = NULL, .size = 0 };
                case OOM_STRATEGY_GROW:
                case OOM_STRATEGY_GROW_IF_POSSIBLE:
                    if (stack_resize_pool(stack, required_size) != 0) {
                        if (stack->allocator.flag.oom_strategy == OOM_STRATEGY_GROW_IF_POSSIBLE) {
                            return (struct MemoryRegion){ .base = NULL, .size = 0 };
                        }
                        panic("StackAllocator: Failed to grow memory pool!");
                    }
                    break;
                default:
                    panic("StackAllocator: Invalid OOM strategy!");
            }
        }
    }

    void *base = stack->pool + aligned_used;
    assert(((uintptr_t)base % alignment) == 0);
    stack->used = required_size;
    return (struct MemoryRegion){ .base = base, .size = size };
}

static void stack_allocator_destroy(struct Allocator *allocator) {
    struct StackAllocator *stack = (struct StackAllocator *)allocator;

    if (stack->pool == NULL) {
        return;
    }

    const struct MemoryRegion pool_region = { .base = stack->pool, .size = stack->pool_size };
    deallocate_from(stack->allocator.parent, pool_region);
    stack->pool = NULL;
    stack->pool_size = 0;
    stack->used = 0;
}

struct StackAllocator make_stack_allocator(struct AllocatorOptions options) {
    const uint8_t oom_strategy = options.oom_strategy != 0
            ? options.oom_strategy
            : OOM_STRATEGY_PANIC;
    const uint32_t alignment = options.alignment != 0
            ? options.alignment
            : (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT;

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
        .pool = NULL,
        .pool_size = 0,
        .used = 0
    };
}

