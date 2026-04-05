#include "system_allocator.h"

#include <stdio.h>
#include <stdlib.h>

static void panic(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static struct MemoryRegion system_allocate(struct Allocator *allocator, size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        switch (allocator->flag.oom_strategy) {
            case OOM_STRATEGY_PANIC:
                panic("SystemAllocator: Out of memory!");
            case OOM_STRATEGY_NULL:
                return (struct MemoryRegion){ .base = NULL, .size = 0 };
            default:
                panic("SystemAllocator: Invalid OOM strategy!");
        }
    }

    return (struct MemoryRegion){ .base = ptr, .size = size };
}

static struct MemoryRegion system_reallocate(struct Allocator *allocator, struct MemoryRegion region, size_t new_size) {
    void *ptr = realloc(region.base, new_size);
    if (ptr == NULL) {
        switch (allocator->flag.oom_strategy) {
            case OOM_STRATEGY_PANIC:
                panic("SystemAllocator: Out of memory during reallocation!");
            case OOM_STRATEGY_NULL:
                return (struct MemoryRegion){ .base = NULL, .size = 0 };
            default:
                panic("SystemAllocator: Invalid OOM strategy!");
        }
    }

    return (struct MemoryRegion){ .base = ptr, .size = new_size };
}

static void system_free(struct Allocator *allocator, struct MemoryRegion region) {
    (void)allocator;
    free(region.base);
}

static void system_destroy(struct Allocator *allocator) {
    (void)allocator;
}

struct SystemAllocator make_system_allocator(struct SystemAllocatorOptions options) {
    return (struct SystemAllocator){
        .allocator = {
            .allocate = system_allocate,
            .reallocate = system_reallocate,
            .deallocate = system_free,
            .destroy = system_destroy,
            .name = options.name ? options.name : "SystemAllocator",
            .parent = options.parent.id != 0 ? options.parent : MAIN_ALLOCATOR_HANDLE,
            .flag = {
                .oom_strategy = options.oom_strategy,
                .supports_reallocation = 1,
                .supports_deallocation = 1,
                .is_thread_safe = 1,
                .alignment = (uint32_t)ALLOCATOR_DEFAULT_ALIGNMENT
            },
            .size = (uint32_t)(sizeof(struct SystemAllocator) - sizeof(struct Allocator))
        }
    };
}

