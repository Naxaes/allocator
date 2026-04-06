#include "allocator.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

const struct AllocatorHandle MAIN_ALLOCATOR_HANDLE = { .id = 0 };

/// A stack of variadic-length allocators.
static uint8_t* allocators = NULL;
/// The top of the stack.
static uint32_t allocator_stack_top = 0;
/// The capacity of the allocators buffer in bytes.
static uint32_t allocator_capacity = 0;
/// The offset in bytes to the base of the last pushed allocator.
static uint32_t allocator_current = 0;


struct AllocatorAlignmentProbe {
    char padding;
    struct Allocator value;
};
#define ALLOCATOR_RECORD_ALIGNMENT ((size_t)offsetof(struct AllocatorAlignmentProbe, value))


static struct AllocatorHandle current_allocator_handle(void) {
    assert(allocator_stack_top != 0 && "No allocator present");
    return (struct AllocatorHandle){ .id = allocator_current };
}

static int allocator_handle_is_main(struct AllocatorHandle handle) {
    return handle.id == MAIN_ALLOCATOR_HANDLE.id;
}

static int allocators_grow(uint32_t required_capacity) {
    uint32_t new_capacity = (allocator_capacity == 0) ? 64U : allocator_capacity;

    while (new_capacity < required_capacity) {
        assert(new_capacity <= UINT32_MAX / 2U);
        new_capacity *= 2U;
    }

    uint8_t* new_allocators = realloc(allocators, new_capacity);
    if (new_allocators == NULL) {
        return -1;
    }

    allocators = new_allocators;
    allocator_capacity = new_capacity;
    return 0;
}

struct AllocatorHandle push_allocator(const struct Allocator* allocator) {
    assert(allocator != NULL);

    const uint32_t total_size = total_size_of_allocator(allocator);
    const uint32_t aligned_offset = align_up(allocator_stack_top, ALLOCATOR_RECORD_ALIGNMENT);
    const uint32_t new_stack_top = aligned_offset + total_size;

    if (new_stack_top > allocator_capacity) {
        const int result = allocators_grow(new_stack_top);
        assert(result == 0 && "Failed to grow allocator registry");
    }

    const struct AllocatorHandle previous_handle = (struct AllocatorHandle){ .id = allocator_current };
    const struct AllocatorHandle handle = { .id = aligned_offset };
    memcpy(&allocators[aligned_offset], allocator, total_size);

    struct Allocator* pushed_allocator = (struct Allocator *)&allocators[aligned_offset];
    pushed_allocator->previous = previous_handle;

    allocator_current = aligned_offset;
    allocator_stack_top = new_stack_top;
    return handle;
}

struct Allocator* get_current_allocator(void) {
    const struct AllocatorHandle handle = current_allocator_handle();
    assert(!allocator_handle_is_main(handle));
    return get_allocator(handle);
}

struct Allocator* get_allocator(struct AllocatorHandle handle) {
    assert(handle.id + sizeof(struct Allocator) <= allocator_stack_top);
    assert((handle.id % ALLOCATOR_RECORD_ALIGNMENT) == 0);
    return (struct Allocator *)&allocators[handle.id];
}

void pop_allocator(void) {
    assert(allocator_stack_top != 0 && "No allocator to pop");
    const struct AllocatorHandle handle = current_allocator_handle();
    assert(handle.id == allocator_current && "Only the current allocator can be popped");

    struct Allocator* allocator = get_allocator(handle);
    const struct AllocatorHandle previous = allocator->previous;
    allocator->destroy(allocator);

    allocator_stack_top = allocator_current;
    allocator_current = previous.id;
}

void allocator_cleanup(void) {
    while (allocator_stack_top != 0) {
        pop_allocator();
    }

    free(allocators);
    allocators = NULL;
    allocator_stack_top = 0;
    allocator_capacity = 0;
}

struct MemoryRegion allocate_from(struct AllocatorHandle handle, size_t size) {
    assert(size > 0);
    struct Allocator* allocator = get_allocator(handle);
    return allocator->allocate(allocator, size);
}

struct MemoryRegion reallocate_from(struct AllocatorHandle handle, struct MemoryRegion region, size_t new_size) {
    assert(new_size > 0);
    struct Allocator* allocator = get_allocator(handle);
    assert(allocator->flag.supports_reallocation && "Allocator doesn't support reallocation!");
    return allocator->reallocate(allocator, region, new_size);
}

void deallocate_from(struct AllocatorHandle handle, struct MemoryRegion region) {
    struct Allocator* allocator = get_allocator(handle);
    assert(allocator->flag.supports_deallocation && "Allocator doesn't support deallocation!");
    allocator->deallocate(allocator, region);
}

struct MemoryRegion allocate(size_t size) {
    return allocate_from(current_allocator_handle(), size);
}

struct MemoryRegion reallocate(struct MemoryRegion region, size_t new_size) {
    return reallocate_from(current_allocator_handle(), region, new_size);
}

void deallocate(struct MemoryRegion region) {
    deallocate_from(current_allocator_handle(), region);
}

