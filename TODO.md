# TODO

## Core allocator system
- Allocator records stored in the global registry are not alignment-safe.
- Handle validation is too weak and does not prove that a handle refers to a live allocator record.
- Size and capacity arithmetic can overflow in allocator registry bookkeeping.
- Critical runtime safety depends on `assert`, which disappears in release builds.
- The allocator system relies on global mutable state, making thread-safety unclear and fragile.
- Public allocator metadata currently exposes internal bookkeeping details such as `previous` and layout-dependent sizing.
- `MAIN_ALLOCATOR_HANDLE` is used as a sentinel even though its name suggests a real allocator.
- `push_allocator()` copies allocator objects by value, which makes ownership and mutation semantics easy to misunderstand.

## Allocation API contracts
- The `reallocate` contract is easy to misuse because failure can lose the new result while the old allocation still remains live.
- Ownership rules for regions and allocator lifetimes are not documented clearly enough in the public API.
- `deallocate_from()` trusts callers to pass a region owned by the given allocator, but that ownership is not validated.

## Stack allocator
- Stack allocations do not guarantee proper alignment for arbitrary object types.
- Stack allocator growth arithmetic can overflow.
- Stack allocator lifetime rules are easy to misuse because all previously returned regions become invalid when the allocator is destroyed or popped.

## System allocator
- Static analysis is already warning about the current `realloc` usage pattern.
- The system allocator wrapper currently adds no allocator-specific state, which makes the abstraction feel inconsistent.

