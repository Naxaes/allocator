#include "allocator.h"

#include <assert.h>
#include <stddef.h>


static struct Allocator* current_allocator = NULL;


void push_allocator(struct Allocator* allocator) {
    assert(allocator != NULL);
    allocator->parent = current_allocator;
    current_allocator = allocator;

}

struct Allocator* get_current_allocator(void) {
    return current_allocator;
}

void pop_allocator(void) {
    assert(current_allocator != NULL);
    if (current_allocator->destroy) {
        current_allocator->destroy(current_allocator);
    }
    current_allocator = current_allocator->parent;
}


struct MemoryRegion allocate_from(struct Allocator* allocator, size_t size) {
    assert(allocator != NULL);
    assert(size > 0);
    return allocator->allocate(allocator, size);
}

struct MemoryRegion reallocate_from(struct Allocator* allocator, struct MemoryRegion region, size_t new_size) {
    assert(allocator != NULL);
    assert(new_size > 0);
    assert(allocator->reallocate != NULL && "Allocator doesn't support reallocation!");
    return allocator->reallocate(allocator, region, new_size);
}

void deallocate_from(struct Allocator* allocator, struct MemoryRegion region) {
    assert(allocator != NULL);
    assert(allocator->deallocate != NULL && "Allocator doesn't support deallocation!");
    allocator->deallocate(allocator, region);
}

struct MemoryRegion allocate(size_t size) {
    struct Allocator* allocator = get_current_allocator();
    assert(allocator != NULL && "No current allocator has been pushed!");
    return allocate_from(allocator, size);
}

struct MemoryRegion reallocate(struct MemoryRegion region, size_t new_size) {
    struct Allocator* allocator = get_current_allocator();
    assert(allocator != NULL && "No current allocator has been pushed!");
    return reallocate_from(allocator, region, new_size);
}

void deallocate(struct MemoryRegion region) {
    struct Allocator* allocator = get_current_allocator();
    assert(allocator != NULL && "No current allocator has been pushed!");
    deallocate_from(allocator, region);
}

