#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "allocator.h"
#include "stack_allocator.h"
#include "system_allocator.h"

static void print_region(const char *label, struct MemoryRegion region) {
    printf("%s at %p with size %zu\n", label, region.base, region.size);
}

static void assert_region_alignment(struct MemoryRegion region, const struct Allocator *allocator) {
    const size_t alignment = allocator->flag.alignment != 0
            ? allocator->flag.alignment
            : ALLOCATOR_DEFAULT_ALIGNMENT;

    assert(region.base != NULL);
    assert(((uintptr_t)region.base % alignment) == 0);
}


int main(void) {
    const size_t startup_allocations[] = { 1, 3, 5, 1024, 2048, 4096 };

    struct SystemAllocator system_allocator = make_system_allocator((struct AllocatorOptions){ 0 });
    printf("System allocator created!\n");
    push_allocator(&system_allocator.allocator);

    struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
        .parent = &system_allocator.allocator,
        .oom_strategy = OOM_STRATEGY_GROW,
    });
    push_allocator(&stack_allocator.allocator);

    for (size_t i = 0; i < sizeof(startup_allocations) / sizeof(startup_allocations[0]); ++i) {
        const struct MemoryRegion region = allocate_from(&stack_allocator.allocator, startup_allocations[i]);
        assert_region_alignment(region, &stack_allocator.allocator);
        print_region("Allocated memory", region);
    }

    const struct MemoryRegion current_region = allocate(512);
    assert_region_alignment(current_region, get_current_allocator());
    print_region("Allocated memory from current allocator", current_region);

    pop_allocator();
    pop_allocator();
    return 0;
}
