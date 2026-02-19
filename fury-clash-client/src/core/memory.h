#pragma once
#include <stddef.h>

/* Simple linear arena allocator — zero fragmentation, O(1) alloc */
typedef struct Arena {
    unsigned char *base;
    size_t         offset;
    size_t         capacity;
} Arena;

void  arena_init(Arena *a, size_t capacity);
void *arena_alloc(Arena *a, size_t size);
void *arena_alloc_aligned(Arena *a, size_t size, size_t align);
void  arena_reset(Arena *a);   /* free all, reuse memory */
void  arena_destroy(Arena *a);
