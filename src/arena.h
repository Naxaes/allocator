#ifndef ALLOCATORS_ARENA_H
#define ALLOCATORS_ARENA_H

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "allocators.h"


#define ARENA_DEFAULT_BLOCK_SIZE ((size_t)1024)
#define ARENA_BLOCK_ALIGNMENT ((size_t)16)

typedef struct ArenaBlock {
    struct ArenaBlock* prev;
    struct ArenaBlock* next;
    void*              base;
    size_t             size;
    size_t             used;
} ArenaBlock;

typedef struct ArenaAllocator {
    Allocator*  parent;
    ArenaBlock* first;
    ArenaBlock* current;
} ArenaAllocator;

struct ArenaAllocatorOptions {
    int reserved;
    Allocator* parent;
};

#define arena_make(...) arena_make_with_options((struct ArenaAllocatorOptions){ 0, __VA_ARGS__ })

static ArenaAllocator arena_make_with_options(struct ArenaAllocatorOptions options) {
    return (ArenaAllocator) {
        .parent = options.parent,
        .first = NULL,
        .current = NULL,
    };
}

static Memory arena_allocate(Allocator* allocator, size_t size, size_t alignment);
static Memory arena_reallocate(Allocator* allocator, Memory memory, size_t new_size, size_t alignment);
static int arena_deallocate(Allocator* allocator, Memory memory);
static void arena_destroy(Allocator* allocator);


static int arena_is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

static size_t arena_max_size(size_t left, size_t right) {
    return left > right ? left : right;
}

static int arena_add_overflow(size_t left, size_t right, size_t* result) {
    if (left > SIZE_MAX - right) {
        return 1;
    }

    *result = left + right;
    return 0;
}

static uintptr_t arena_align_forward(uintptr_t value, size_t alignment) {
    return (value + (uintptr_t)(alignment - 1)) & ~((uintptr_t)alignment - 1);
}

static size_t arena_block_payload_alignment(size_t alignment) {
    return arena_max_size(alignment, ARENA_BLOCK_ALIGNMENT);
}

static int arena_required_block_size(size_t size, size_t alignment, size_t* result) {
    if (size == 0) {
        *result = 0;
        return 0;
    }

    return arena_add_overflow(size, alignment - 1, result);
}

static size_t arena_next_block_size(ArenaAllocator* arena, size_t required_size) {
    size_t block_size = ARENA_DEFAULT_BLOCK_SIZE;

    if (arena->current != NULL && arena->current->size > 0) {
        block_size = arena->current->size;
        if (block_size <= SIZE_MAX / 2) {
            block_size *= 2;
        }
    }

    while (block_size < required_size && block_size <= SIZE_MAX / 2) {
        block_size *= 2;
    }

    return arena_max_size(block_size, required_size);
}

static void arena_link_block(ArenaAllocator* arena, ArenaBlock* block) {
    block->prev = arena->current;
    block->next = NULL;

    if (arena->current != NULL) {
        arena->current->next = block;
    } else {
        arena->first = block;
    }

    arena->current = block;
}

static void arena_unlink_block(ArenaAllocator* arena, ArenaBlock* block) {
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        arena->first = block->next;
    }

    if (block->next != NULL) {
        block->next->prev = block->prev;
    }

    if (arena->current == block) {
        arena->current = block->prev;
    }

    block->prev = NULL;
    block->next = NULL;
}

static void arena_destroy_block(ArenaAllocator* arena, ArenaBlock* block) {
    Memory payload = { .base = block->base, .size = block->size };
    Memory header = { .base = block, .size = sizeof(*block) };

    deallocate(arena->parent, payload);
    deallocate(arena->parent, header);
}

static ArenaBlock* arena_append_block(ArenaAllocator* arena, size_t size, size_t alignment) {
    size_t required_size = 0;
    if (arena_required_block_size(size, alignment, &required_size) != 0) {
        return NULL;
    }

    size_t block_size = arena_next_block_size(arena, required_size);
    size_t block_alignment = arena_block_payload_alignment(alignment);

    Memory header_memory = allocate(arena->parent, sizeof(ArenaBlock), ARENA_BLOCK_ALIGNMENT);
    if (header_memory.base == NULL) {
        return NULL;
    }

    Memory payload_memory = allocate(arena->parent, block_size, block_alignment);
    if (payload_memory.base == NULL) {
        deallocate(arena->parent, header_memory);
        return NULL;
    }

    ArenaBlock* block = (ArenaBlock*)header_memory.base;
    block->prev = NULL;
    block->next = NULL;
    block->base = payload_memory.base;
    block->size = payload_memory.size;
    block->used = 0;

    arena_link_block(arena, block);
    return block;
}

static int arena_block_contains_memory(const ArenaBlock* block, Memory memory) {
    uintptr_t block_begin = (uintptr_t)block->base;
    uintptr_t block_end = block_begin + block->size;
    uintptr_t memory_begin = (uintptr_t)memory.base;
    uintptr_t memory_end = memory_begin + memory.size;

    return memory_begin >= block_begin && memory_end <= block_end;
}

static int arena_block_is_tail_allocation(const ArenaBlock* block, Memory memory, size_t* allocation_offset) {
    uintptr_t block_begin = (uintptr_t)block->base;
    uintptr_t memory_begin = (uintptr_t)memory.base;
    uintptr_t memory_end = memory_begin + memory.size;

    if (!arena_block_contains_memory(block, memory)) {
        return 0;
    }

    if (memory_end != block_begin + block->used) {
        return 0;
    }

    *allocation_offset = (size_t)(memory_begin - block_begin);
    return 1;
}

static Memory arena_allocate_from_block(ArenaBlock* block, size_t size, size_t alignment) {
    uintptr_t raw = (uintptr_t)block->base + block->used;
    uintptr_t aligned = arena_align_forward(raw, alignment);
    size_t padding = (size_t)(aligned - raw);
    size_t consumed = 0;

    if (arena_add_overflow(size, padding, &consumed) != 0) {
        return MEMORY_NULL;
    }

    if (consumed > block->size - block->used) {
        return MEMORY_NULL;
    }

    block->used += consumed;
    return (Memory) { .base = (void*)aligned, .size = size };
}

static void arena_trim_empty_current_block(ArenaAllocator* arena) {
    while (arena->current != NULL && arena->current->used == 0 && arena->current->prev != NULL) {
        ArenaBlock* empty = arena->current;
        arena_unlink_block(arena, empty);
        arena_destroy_block(arena, empty);
    }
}

static Memory arena_allocate(Allocator* allocator, size_t size, size_t alignment) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;

    assert(arena_is_power_of_two(alignment));

    if (size == 0) {
        return MEMORY_NULL;
    }

    if (arena->current != NULL) {
        Memory result = arena_allocate_from_block(arena->current, size, alignment);
        if (result.base != NULL) {
            return result;
        }
    }

    ArenaBlock* block = arena_append_block(arena, size, alignment);
    if (block == NULL) {
        return MEMORY_NULL;
    }

    return arena_allocate_from_block(block, size, alignment);
}

static Memory arena_reallocate(Allocator* allocator, Memory memory, size_t new_size, size_t alignment) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;

    assert(arena_is_power_of_two(alignment));

    if (memory.base == NULL) {
        return arena_allocate(allocator, new_size, alignment);
    }

    if (new_size == 0) {
        (void)arena_deallocate(allocator, memory);
        return MEMORY_NULL;
    }

    if (arena->current != NULL) {
        size_t allocation_offset = 0;
        if (arena_block_is_tail_allocation(arena->current, memory, &allocation_offset)) {
            size_t new_used = 0;
            if (arena_add_overflow(allocation_offset, new_size, &new_used) == 0 && new_used <= arena->current->size) {
                arena->current->used = new_used;
                return (Memory) { .base = memory.base, .size = new_size };
            }

            ArenaBlock* old_block = arena->current;
            Memory result = arena_allocate(allocator, new_size, alignment);
            if (result.base == NULL) {
                return MEMORY_NULL;
            }

            memcpy(result.base, memory.base, memory.size < new_size ? memory.size : new_size);
            old_block->used = allocation_offset;

            if (old_block->used == 0 && old_block != arena->current) {
                arena_unlink_block(arena, old_block);
                arena_destroy_block(arena, old_block);
            }

            return result;
        }
    }

    for (ArenaBlock* block = arena->first; block != NULL; block = block->next) {
        if (arena_block_contains_memory(block, memory)) {
            return MEMORY_NULL;
        }
    }

    assert(0 && "Attempting to reallocate memory that was not allocated by this arena");
    return MEMORY_NULL;
}

static int arena_deallocate(Allocator* allocator, Memory memory) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;

    if (memory.base == NULL) {
        return 1;
    }

    if (arena->current != NULL) {
        size_t allocation_offset = 0;
        if (arena_block_is_tail_allocation(arena->current, memory, &allocation_offset)) {
            arena->current->used = allocation_offset;
            arena_trim_empty_current_block(arena);
            return 1;
        }
    }

    for (ArenaBlock* block = arena->first; block != NULL; block = block->next) {
        size_t allocation_offset = 0;
        if (arena_block_contains_memory(block, memory)) {
            if (arena_block_is_tail_allocation(block, memory, &allocation_offset)) {
                arena->current->used = allocation_offset;
                arena_trim_empty_current_block(arena);
                return 1;
            } else {
                return 0;
            }
        }
    }

    assert(0 && "Attempting to deallocate memory that was not allocated by this arena");
    return 0;
}

static void arena_destroy(Allocator* allocator) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;

    ArenaBlock* block = arena->first;
    while (block != NULL) {
        ArenaBlock* next = block->next;
        arena_destroy_block(arena, block);
        block = next;
    }

    arena->first = NULL;
    arena->current = NULL;
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
