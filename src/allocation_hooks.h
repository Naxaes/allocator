#ifndef ALLOCATION_HOOKS_H
#define ALLOCATION_HOOKS_H
#include <stdio.h>

#include "allocator_base.h"


size_t time_ns(void);

#define ALLOCATOR_PRE_ALLOCATE_HOOK(allocator, size, alignment, file, function, line) size_t start_time = time_ns()
#define ALLOCATOR_POST_ALLOCATE_HOOK(allocator, size, alignment, result, file, function, line) allocation_post_allocate_hook(allocator, size, alignment, result, file, function, line, time_ns() - start_time)

#define ALLOCATOR_POST_REALLOCATE_HOOK(allocator, memory, new_size, alignment, result, file, function, line) allocation_post_realloc_hook(allocator, memory, new_size, alignment, result, file, function, line)
#define ALLOCATOR_POST_DEALLOCATE_HOOK(allocator, memory, file, function, line) allocation_post_deallocate_hook(allocator, memory, file, function, line)
#define ALLOCATOR_POST_DESTROY_HOOK(allocator, file, function, line) allocation_post_destroy_hook(allocator, file, function, line)

int  allocation_hook_init(const char* path);
void allocation_hook_deinit(void);
void allocation_post_allocate_hook(Allocator allocator, size_t size, size_t alignment, Memory result, const char* file, const char* function, int line, size_t time_ns);
void allocation_post_realloc_hook(Allocator allocator, Memory memory, size_t new_size, size_t alignment, Memory result, const char* file, const char* function, int line);
void allocation_post_deallocate_hook(Allocator allocator, Memory memory, const char* file, const char* function, int line);
void allocation_post_destroy_hook(Allocator allocator, const char* file, const char* function, int line);


#endif  // ALLOCATION_HOOKS_H


#ifdef ALLOCATION_HOOKS_IMPLEMENTATION

#include <time.h>
#include <assert.h>


static FILE* allocation_log_file = NULL;

int allocation_hook_init(const char* path) {
    if (allocation_log_file != NULL) {
        return 0;
    }
    allocation_log_file = fopen(path, "w");
    if (allocation_log_file == NULL) {
        return -1;
    }
    fprintf(allocation_log_file, "[\n\t{\n\t\t\"kind\": \"hook_init\",\n\t\t\"path\": \"%s\"\n\t}", path);
    return 0;
}

void allocation_hook_deinit() {
    if (allocation_log_file != NULL) {
        fprintf(allocation_log_file, "\n]\n");
        fclose(allocation_log_file);
    }
}

void allocation_post_allocate_hook(Allocator allocator, size_t size, size_t alignment, Memory result, const char* file, const char* function, int line, size_t time_ns) {
    assert(allocation_log_file != NULL && "allocation_post_allocate_hook requires allocation_hook_init to be called");
    static const char* format = ",\n\t{\n"
                         "\t\t\"kind\": \"allocation\",\n"
                         "\t\t\"file\": \"%s\",\n"
                         "\t\t\"line\": \"%d\",\n"
                         "\t\t\"function\": \"%s\",\n"
                         "\t\t\"allocator_kind\": \"%zu\",\n"
                         "\t\t\"allocator_data\": \"%p\",\n"
                         "\t\t\"size\": \"%zu\",\n"
                         "\t\t\"alignment\": \"%zu\",\n"
                         "\t\t\"allocated_size\": \"%zu\",\n"
                         "\t\t\"allocated_ptr\": \"%p\",\n"
                         "\t\t\"time_ns\": \"%zu\"\n"
                         "\t}";
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;

    fprintf(allocation_log_file, format, file, line, function, kind, (void*)data, size, alignment, result.size, result.base, time_ns);
}

void allocation_post_realloc_hook(Allocator allocator, Memory memory, size_t new_size, size_t alignment, Memory result, const char* file, const char* function, int line) {
    assert(allocation_log_file != NULL && "allocation_post_realloc_hook requires allocation_hook_init to be called");
    static const char* format = ",\n\t{\n"
                         "\t\t\"kind\": \"reallocation\",\n"
                         "\t\t\"file\": \"%s\",\n"
                         "\t\t\"line\": \"%d\",\n"
                         "\t\t\"function\": \"%s\",\n"
                         "\t\t\"allocator_kind\": \"%zu\",\n"
                         "\t\t\"allocator_data\": \"%p\",\n"
                         "\t\t\"old_size\": \"%zu\",\n"
                         "\t\t\"old_ptr\": \"%p\",\n"
                         "\t\t\"new_size\": \"%zu\",\n"
                         "\t\t\"new_alignment\": \"%zu\",\n"
                         "\t\t\"allocated_size\": \"%zu\",\n"
                         "\t\t\"allocated_ptr\": \"%p\"\n"
                         "\t}";
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;

    fprintf(allocation_log_file, format, file, line, function, kind, (void*)data, memory.size, memory.base, new_size, alignment, result.size, result.base);
}

void allocation_post_deallocate_hook(Allocator allocator, Memory memory, const char* file, const char* function, int line) {
    assert(allocation_log_file != NULL && "allocation_post_deallocate_hook requires allocation_hook_init to be called");
    static const char* format = ",\n\t{\n"
                         "\t\t\"kind\": \"deallocation\",\n"
                         "\t\t\"file\": \"%s\",\n"
                         "\t\t\"line\": \"%d\",\n"
                         "\t\t\"function\": \"%s\",\n"
                         "\t\t\"allocator_kind\": \"%zu\",\n"
                         "\t\t\"allocator_data\": \"%p\",\n"
                         "\t\t\"deallocated_size\": \"%zu\",\n"
                         "\t\t\"deallocated_ptr\": \"%p\"\n"
                         "\t}";
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;

    fprintf(allocation_log_file, format, file, line, function, kind, (void*)data, memory.size, memory.base);
}

void allocation_post_destroy_hook(Allocator allocator, const char* file, const char* function, int line) {
    assert(allocation_log_file != NULL && "allocation_post_free_hook requires allocation_hook_init to be called");
    static const char* format = ",\n\t{\n"
                         "\t\t\"kind\": \"destroy\",\n"
                         "\t\t\"file\": \"%s\",\n"
                         "\t\t\"line\": \"%d\",\n"
                         "\t\t\"function\": \"%s\",\n"
                         "\t\t\"allocator_kind\": \"%zu\",\n"
                         "\t\t\"allocator_data\": \"%p\"\n"
                         "\t}";
    uint8_t   kind = (uintptr_t)allocator & 15;
    uintptr_t data = (uintptr_t)allocator & 0xFFFFFFFFFFFFFFF0;

    fprintf(allocation_log_file, format, file, line, function, kind, (void*)data);
}


size_t time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (size_t)ts.tv_sec * 1000000000 + (size_t)ts.tv_nsec;
}


#endif  // ALLOCATION_HOOKS_IMPLEMENTATION





