#ifndef ALLOCATORS_ARENA_H
#define ALLOCATORS_ARENA_H

#include <assert.h>

#include "allocators.h"


typedef struct ArenaAllocator {
    Allocator* parent;
    void*      base;
    size_t     size;
    size_t     used;
} ArenaAllocator;

struct ArenaAllocatorOptions {
    int reserved;
    Allocator* parent;
};

#define arena_make(...) arena_make_with_options((struct ArenaAllocatorOptions){ 0, __VA_ARGS__ })

static ArenaAllocator arena_make_with_options(struct ArenaAllocatorOptions options) {
    return (ArenaAllocator) {
        .parent = options.parent,
        .base = NULL,
        .size = 0,
        .used = 0,
    };
}


static Memory arena_allocate(Allocator* allocator, size_t size, size_t alignment) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    size_t offset = (size_t)arena->base + arena->used;
    size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned_offset - offset;

    if (aligned_offset + size > (size_t)arena->base + arena->size) {
        Memory result = MEMORY_NULL;
        if (arena->base == NULL) {
            size_t initial_size = 4096;
            size_t base_alignment = 16;
            result = allocate(arena->parent, initial_size, base_alignment);
            if (result.base == NULL) {
                return MEMORY_NULL;
            }
        } else {
            Memory old_memory = { .base = arena->base, .size = arena->size };
            size_t new_size = arena->size * 2;
            size_t base_alignment = 16;
            result = reallocate(arena->parent, old_memory, new_size, base_alignment);
            if (result.base == NULL) {
                return MEMORY_NULL;
            }
        }
        arena->base = result.base;
        arena->size = result.size;

        offset = (size_t)arena->base + arena->used;
        aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
        padding = aligned_offset - offset;
    }

    arena->used += padding + size;
    return (Memory) { .base = (void*)aligned_offset, .size = size };
}

// Reallocates if the memory points to the last allocated memory, otherwise does nothing.
static Memory arena_reallocate(Allocator* allocator, Memory memory, size_t new_size, size_t alignment) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    size_t end_offset = (size_t)memory.base + memory.size;

    if (end_offset == (size_t)arena->base + arena->used) {
        size_t offset = (size_t)memory.base;
        size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
        size_t padding = aligned_offset - offset;

        if (aligned_offset + new_size > (size_t)arena->base + arena->size) {
            Memory old_memory = { .base = arena->base, .size = arena->size };
            size_t new_arena_size = arena->size * 2;
            size_t base_alignment = 16;
            Memory result = reallocate(arena->parent, old_memory, new_arena_size, base_alignment);
            if (result.base == NULL) {
                return MEMORY_NULL;
            }

            memory.base = result.base;
            memory.size = result.size;

            offset = (size_t)memory.base;
            aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
            padding = aligned_offset - offset;
        }

        arena->used += padding + (new_size - memory.size);
        return (Memory) { .base = (void*)aligned_offset, .size = new_size };
    }

    return MEMORY_NULL;
}

// Deallocates if the memory points to the last allocated memory, otherwise does nothing.
static int arena_deallocate(Allocator* allocator, Memory memory) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    size_t end_offset = (size_t)memory.base + memory.size;

    if (end_offset == (size_t)arena->base + arena->used) {
        arena->used -= memory.size;
        return 1;
    }

    // Check if we own this memory
    if ((size_t)memory.base >= (size_t)arena->base && end_offset <= (size_t)arena->base + arena->size) {
        return 0;
    }

    assert(0 && "Attempting to deallocate memory that was not allocated by this arena");
}

// TODO: If an arena is backed by another arena, we might not be able to destroy and should report that.
static void arena_destroy(Allocator* allocator) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    if (arena->base != NULL) {
        Memory old_memory = { .base = arena->base, .size = arena->size };
        deallocate(arena->parent, old_memory);
    }
}

static int arena_allocator_kind;
static int arena_allocator_initialize(void) {
    arena_allocator_kind = allocator_register_kind((AllocatorFunctionTable) {
        .allocate = arena_allocate,
        .reallocate = arena_reallocate,
        .deallocate = arena_deallocate,
        .destroy = arena_destroy,
    });
    return arena_allocator_kind;
}


#endif //ALLOCATORS_ARENA_H
