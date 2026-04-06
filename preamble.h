#include <stdint.h>
#include <stddef.h>


struct P__AllocatorDefaultAlignmentProbe {
    char padding;
    union {
        void* as_pointer;
        size_t as_size;
        long double as_long_double;
        long long as_long_long;
    } value;
};

#define ALLOCATOR_DEFAULT_ALIGNMENT ((size_t)offsetof(struct P__AllocatorDefaultAlignmentProbe, value))
