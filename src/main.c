#define ALLOCATOR_IMPLEMENTATION
#include <stdio.h>

#include "allocators.h"
#include "arena.h"


int main(void) {
    allocator_register_kind((AllocatorFunctionTable) {
        .allocate = allocate_system,
        .reallocate = reallocate_system,
        .deallocate = deallocate_system,
        .destroy = destroy_system,
    });

    arena_allocator_kind = allocator_register_kind((AllocatorFunctionTable) {
        .allocate = arena_allocate,
        .reallocate = arena_reallocate,
        .deallocate = arena_deallocate,
        .destroy = arena_destroy,
    });

    ArenaAllocator arena = arena_make(.parent = 0);
    Allocator allocator = allocator_to_base(&arena, arena_allocator_kind);

    Memory memory = allocate(allocator, 512, 8);
    printf("%p with size %zu\n", memory.base, memory.size);

    Memory memory2 = allocate(allocator, 512, 8);
    printf("%p with size %zu\n", memory2.base, memory2.size);

    Memory memory3 = allocate(allocator, 512, 8);
    printf("%p with size %zu\n", memory3.base, memory3.size);

    deallocate(allocator, memory);
    deallocate(allocator, memory2);

    destroy_allocator(allocator);

    return 0;
}