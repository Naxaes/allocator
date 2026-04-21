#define ALLOCATION_HOOKS_IMPLEMENTATION
#include "allocation_hooks.h"

#define ALLOCATOR_SYSTEM_IMPLEMENTATION
#include "allocator_system.h"

#include "arena.h"

#define ALLOCATORS_IMPLEMENTATION
#include "allocators.h"

#include <stdio.h>



int main(void) {
    allocation_hook_init("allocation_log.json");

    allocator_register_kind((AllocatorFunctionTable) {
        .allocate = allocate_system,
        .reallocate = reallocate_system,
        .deallocate = deallocate_system,
        .destroy = destroy_system,
        .query = query_system,
    });
    allocator_push(0);

    Memory x = allocate(1025, 8);
    printf("%p with size %zu\n", x.base, x.size);

    arena_allocator_initialize();

    ArenaAllocator arena = arena_make(.parent = 0);
    Allocator* allocator = allocator_to_base(&arena, arena_allocator_kind);

    Memory memory = allocate(allocator, 512, 8);
    printf("%p with size %zu\n", memory.base, memory.size);

    Memory memory2 = allocate(allocator, 512, 8);
    printf("%p with size %zu\n", memory2.base, memory2.size);

    Memory memory3 = allocate(allocator, 512, 8);
    printf("%p with size %zu\n", memory3.base, memory3.size);

    deallocate(allocator, memory);
    deallocate(allocator, memory2);

    destroy_allocator(allocator);

    allocation_hook_deinit();

    return 0;
}
