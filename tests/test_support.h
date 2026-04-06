#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stdint.h>
#include <stdio.h>

#include "allocator.h"

#define TEST_CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            return 1; \
        } \
    } while (0)

static inline int test_region_is_aligned(struct MemoryRegion region, size_t alignment) {
    return region.base != NULL && ((uintptr_t)region.base % alignment) == 0;
}

#endif

