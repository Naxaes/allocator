#include "allocator.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

const struct AllocatorHandle MAIN_ALLOCATOR_HANDLE = { .id = UINT32_MAX };

static uint8_t *allocators = NULL;
static uint32_t allocator_offset = 0;
static uint32_t allocator_capacity = 0;
static struct AllocatorHandle allocator_current = { .id = UINT32_MAX };

struct AllocatorAlignmentProbe {
    char padding;
    struct Allocator value;
};

#define ALLOCATOR_RECORD_ALIGNMENT ((size_t)offsetof(struct AllocatorAlignmentProbe, value))

static size_t allocator_record_alignment(const struct Allocator *allocator) {
    size_t alignment = allocator->flag.alignment;

    if (alignment == 0) {
        alignment = ALLOCATOR_DEFAULT_ALIGNMENT;
    }
    if (alignment < ALLOCATOR_RECORD_ALIGNMENT) {
        alignment = ALLOCATOR_RECORD_ALIGNMENT;
    }

    return alignment;
}

static uint32_t align_allocator_offset(uint32_t offset, size_t alignment) {
    const size_t remainder = offset % alignment;
    size_t aligned_offset = offset;

    if (remainder != 0) {
        aligned_offset += alignment - remainder;
    }

    assert(aligned_offset <= UINT32_MAX);
    return (uint32_t)aligned_offset;
}


static int allocator_handle_is_main(struct AllocatorHandle handle) {
    return handle.id == MAIN_ALLOCATOR_HANDLE.id;
}

static int allocators_grow(uint32_t required_capacity) {
    uint32_t new_capacity = (allocator_capacity == 0) ? 64U : allocator_capacity;

    while (new_capacity < required_capacity) {
        if (new_capacity > UINT32_MAX / 2U) {
            new_capacity = required_capacity;
            break;
        }
        new_capacity *= 2U;
    }

    uint8_t *new_allocators = realloc(allocators, new_capacity);
    if (new_allocators == NULL) {
        return -1;
    }

    allocators = new_allocators;
    allocator_capacity = new_capacity;
    return 0;
}

struct AllocatorHandle push_allocator(const struct Allocator *allocator) {
    assert(allocator != NULL);

    const size_t alignment = allocator_record_alignment(allocator);
    const uint32_t aligned_offset = align_allocator_offset(allocator_offset, alignment);
    const uint32_t total_size = (uint32_t)(sizeof(struct Allocator) + allocator->size);
    const uint32_t required_capacity = aligned_offset + total_size;

    if (required_capacity > allocator_capacity) {
        const int result = allocators_grow(required_capacity);
        assert(result == 0 && "Failed to grow allocator registry");
    }

    const struct AllocatorHandle handle = { .id = aligned_offset };
    memcpy(&allocators[aligned_offset], allocator, total_size);

    struct Allocator *stored_allocator = (struct Allocator *)&allocators[aligned_offset];
    stored_allocator->previous = allocator_current;

    allocator_offset = required_capacity;
    allocator_current = handle;
    return handle;
}

struct Allocator *get_current_allocator(void) {
    assert(!allocator_handle_is_main(allocator_current));
    return get_allocator(allocator_current);
}

struct Allocator *get_allocator(struct AllocatorHandle handle) {
    assert(handle.id + sizeof(struct Allocator) <= allocator_offset);
    assert((handle.id % ALLOCATOR_RECORD_ALIGNMENT) == 0);
    return (struct Allocator *)&allocators[handle.id];
}

void pop_allocator(struct AllocatorHandle handle) {
    assert(!allocator_handle_is_main(handle));
    assert(handle.id == allocator_current.id && "Only the current allocator can be popped");

    struct Allocator *allocator = get_allocator(handle);
    allocator->destroy(allocator);

    allocator_offset = handle.id;
    allocator_current = allocator->previous;
}

void allocator_cleanup(void) {
    while (!allocator_handle_is_main(allocator_current)) {
        struct Allocator *allocator = get_current_allocator();
        const struct AllocatorHandle current = allocator_current;

        allocator->destroy(allocator);
        allocator_offset = current.id;
        allocator_current = allocator->previous;
    }

    free(allocators);
    allocators = NULL;
    allocator_offset = 0;
    allocator_capacity = 0;
}

struct MemoryRegion allocate_from(struct AllocatorHandle handle, size_t size) {
    assert(size > 0);
    struct Allocator *allocator = get_allocator(handle);
    return allocator->allocate(allocator, size);
}

struct MemoryRegion reallocate_from(struct AllocatorHandle handle, struct MemoryRegion region, size_t new_size) {
    assert(new_size > 0);
    struct Allocator *allocator = get_allocator(handle);
    assert(allocator->flag.supports_reallocation && "Allocator doesn't support reallocation!");
    return allocator->reallocate(allocator, region, new_size);
}

void deallocate_from(struct AllocatorHandle handle, struct MemoryRegion region) {
    struct Allocator *allocator = get_allocator(handle);
    assert(allocator->flag.supports_deallocation && "Allocator doesn't support deallocation!");
    allocator->deallocate(allocator, region);
}

struct MemoryRegion allocate(size_t size) {
    return allocate_from(allocator_current, size);
}

struct MemoryRegion reallocate(struct MemoryRegion region, size_t new_size) {
    return reallocate_from(allocator_current, region, new_size);
}

void deallocate(struct MemoryRegion region) {
    deallocate_from(allocator_current, region);
}

