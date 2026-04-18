#ifndef ALLOCATORS_ARENA_H
#define ALLOCATORS_ARENA_H



static int arena_allocator_kind;

typedef struct ArenaAllocator {
    Allocator parent;
    void*     base;
    size_t    size;
    size_t    used;
} ArenaAllocator;

struct ArenaAllocatorOptions {
    int reserved;
    Allocator parent;
};

#define arena_make(...) arena_make_with_options((struct ArenaAllocatorOptions){ 0, __VA_ARGS__ })

ArenaAllocator arena_make_with_options(struct ArenaAllocatorOptions options) {
    return (ArenaAllocator){
        .parent = options.parent,
        .base = NULL,
        .size = 0,
        .used = 0,
    };
}


Memory arena_allocate(Allocator allocator, size_t size, size_t alignment) {
    ArenaAllocator* arena = (ArenaAllocator*)allocator;
    size_t offset = (size_t)arena->base + arena->used;
    size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned_offset - offset;

    if (aligned_offset + size > (size_t)arena->base + arena->size) {
        Memory old_memory = { .base = arena->base, .size = arena->size };
        size_t new_size = arena->size * 2;
        size_t base_alignment = 16;
        Memory result = reallocate(allocator, old_memory, new_size, base_alignment);
        if (result.base == NULL) {
            return (Memory){ 0 };
        }
    }

    arena->used += padding + size;
    return (Memory){ .base = (void*)aligned_offset, .size = size };
}

Memory arena_reallocate(Allocator allocator, Memory memory, size_t new_size, size_t alignment) {
    (void)allocator;
    (void)memory;
    (void)new_size;
    (void)alignment;
    return (Memory){ 0 };
}

void arena_deallocate(Allocator allocator, Memory memory) {
    (void)allocator;
    (void)memory;
}

void arena_destroy(Allocator allocator) {
    (void)allocator;
}


#endif //ALLOCATORS_ARENA_H