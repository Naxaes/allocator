#include "system_allocator.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void panic(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static size_t system_alignment_bytes(const struct Allocator *allocator) {
    return allocator_alignment_bytes_for(allocator);
}

static struct MemoryRegion system_oom_result(struct Allocator *allocator, const char *message) {
    switch (allocator->flag.oom_strategy) {
        case OOM_STRATEGY_PANIC:
            panic(message);
        case OOM_STRATEGY_NULL:
            return (struct MemoryRegion){ .base = NULL, .size = 0 };
        default:
            panic("SystemAllocator: Invalid OOM strategy!");
    }

    return (struct MemoryRegion){ .base = NULL, .size = 0 };
}

static struct MemoryRegion system_allocate(struct Allocator *allocator, size_t size) {
    const size_t alignment = system_alignment_bytes(allocator);
    void *ptr = NULL;

    if (alignment <= ALLOCATOR_DEFAULT_ALIGNMENT) {
        ptr = malloc(size);
    } else if (posix_memalign(&ptr, alignment, size) != 0) {
        ptr = NULL;
    }

    if (ptr == NULL) {
        return system_oom_result(allocator, "SystemAllocator: Out of memory!");
    }

    return (struct MemoryRegion){ .base = ptr, .size = size };
}

static struct MemoryRegion system_reallocate(struct Allocator *allocator, struct MemoryRegion region, size_t new_size) {
    const size_t alignment = system_alignment_bytes(allocator);

    if (alignment <= ALLOCATOR_DEFAULT_ALIGNMENT) {
        void *ptr = realloc(region.base, new_size);
        if (ptr == NULL) {
            return system_oom_result(allocator, "SystemAllocator: Out of memory during reallocation!");
        }

        return (struct MemoryRegion){ .base = ptr, .size = new_size };
    }

    {
        struct MemoryRegion new_region = system_allocate(allocator, new_size);
        const size_t bytes_to_copy = region.size < new_size ? region.size : new_size;

        if (new_region.base == NULL) {
            return new_region;
        }

        if (region.base != NULL && bytes_to_copy > 0) {
            memcpy(new_region.base, region.base, bytes_to_copy);
        }
        free(region.base);
        return new_region;
    }
}

static void system_free(struct Allocator *allocator, struct MemoryRegion region) {
    (void)allocator;
    free(region.base);
}

static void system_destroy(struct Allocator *allocator) {
    (void)allocator;
}

struct SystemAllocator make_system_allocator(struct AllocatorOptions options) {
    const uint8_t oom_strategy = options.oom_strategy != 0 ? options.oom_strategy : OOM_STRATEGY_PANIC;
    const uint32_t alignment = allocator_normalize_alignment_exponent(options.alignment);

    return (struct SystemAllocator){
        .allocator = {
            .allocate = system_allocate,
            .reallocate = system_reallocate,
            .deallocate = system_free,
            .destroy = system_destroy,
            .parent = options.parent ? options.parent : get_current_allocator(),
            .flag = {
                .oom_strategy = oom_strategy,
                .is_thread_safe = 1,
                .alignment = alignment,
                .size = (uint32_t)(sizeof(struct SystemAllocator) - sizeof(struct Allocator))
            },
        }
    };
}

