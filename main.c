#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "allocator.h"
#include "stack_allocator.h"
#include "system_allocator.h"

static void print_region(const char *label, struct MemoryRegion region) {
    printf("%s at %p with size %zu\n", label, region.base, region.size);
}

static void assert_region_alignment(struct MemoryRegion region) {
    assert(region.base != NULL);
    assert(((uintptr_t)region.base % ALLOCATOR_DEFAULT_ALIGNMENT) == 0);
}


int main(void) {
    const size_t startup_allocations[] = { 1, 3, 5, 1024, 2048, 4096 };

    const struct SystemAllocator system_allocator = make_system_allocator((struct SystemAllocatorOptions){ 0 });
    printf("System allocator created!\n");
    const struct AllocatorHandle system_allocator_handle = push_allocator(&system_allocator.allocator);

    const struct StackAllocator stack_allocator = make_stack_allocator(system_allocator_handle, OOM_STRATEGY_GROW);
    const struct AllocatorHandle stack_handle = push_allocator(&stack_allocator.allocator);
    printf("Pushed stack allocator with handle id %u\n", stack_handle.id);

    for (size_t i = 0; i < sizeof(startup_allocations) / sizeof(startup_allocations[0]); ++i) {
        const struct MemoryRegion region = allocate_from(stack_handle, startup_allocations[i]);
        assert_region_alignment(region);
        print_region("Allocated memory", region);
    }

    const struct MemoryRegion current_region = allocate(512);
    assert_region_alignment(current_region);
    print_region("Allocated memory from current allocator", current_region);

    pop_allocator();
    allocator_cleanup();
    return 0;
}
