
# Design

An allocator must have an `allocate` function and a `destroy` function.
If `deallocate` is not supported, the allocator will not free memory until `destroy` is called.
If `reallocate` is not supported, the allocator will either:
    1. If `deallocate` is supported, allocate a new block of memory and deallocate the old one.
    2. If `deallocate` is not supported, allocate a new block of memory and let it leak until `destroy` is called.

```c++
struct BaseAllocator {
    AllocateFn allocate;
    DestroyFn  destroy;
    uint32_t   supports_reallocation : 1,
               supports_deallocation : 1,
               oom_strategy : 2,
               alignment : 16,
               is_thread_safe : 1,
               id : 11;
    uint32_t size;
};

struct ReallocAllocator {
    BaseAllocator base;
    ReallocateFn reallocate;
};

struct DeallocAllocator {
    BaseAllocator base;
    DeallocateFn deallocate;
};

struct FullAllocator {
    BaseAllocator base;
    ReallocateFn reallocate;
    DeallocateFn deallocate;
};




struct StackAllocator {
    BaseAllocator base;
    uint8_t* buffer;
    size_t offset;
};

struct PoolAllocator {
    DeallocAllocator base;
    uint8_t* buffer;
    size_t block_size;
    size_t block_count;
    uint8_t* free_list_head;
};

struct FreeListAllocator {
    DeallocAllocator base;
    uint8_t* buffer;
    size_t block_size;
    size_t block_count;
    uint8_t* free_list_head;
};

struct HeapAllocator {
    FullAllocator base;
    uint8_t* buffer;
    size_t capacity;
    size_t used;
};
```


```c++

/// A stack of variadic-length allocators with 64-byte granularity.
static uint8_t* allocators = NULL;
/// The top of the stack.
static uint32_t allocator_stack_top = 0;
/// The capacity of the allocators buffer in bytes.
static uint32_t allocator_capacity = 0;
/// The offset in bytes to the base of the last pushed allocator.
static uint32_t allocator_current = 0;

// Handle to an allocator.
typedef Allocator uint32_t;

struct AllocatorData {
    AllocateFn   allocate;
    ReallocateFn reallocate;
    DeallocateFn deallocate;
    DestroyFn    destroy; 
    uint64_t size           : 8,    // Size of the allocator struct in 64 bytes, excluding the header.
             alignment      : 6,    // The power of two alignment of the allocator. For example, an alignment of 4 means 16 byte alignment.
             oom_strategy   : 2,      
             is_thread_safe : 1,
             unused         : 7,
             parent         : 20,   // Offset in granularity of 64 bytes from the allocator stack base. 
             previous       : 20;   // Offset in granularity of 64 bytes from the allocator stack base.
    
    /* --- Implementation data, maximum of 2^8 * 64 = 16KB --- */    
};


struct Array {
    Allocator allocator;
    uint8_t*  buffer;
    size_t    length;
    size_t    capacity;
};









```