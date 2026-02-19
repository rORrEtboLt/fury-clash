#include "memory.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

void arena_init(Arena *a, size_t capacity) {
    a->base     = SDL_malloc(capacity);
    a->offset   = 0;
    a->capacity = a->base ? capacity : 0;
}

void *arena_alloc(Arena *a, size_t size) {
    return arena_alloc_aligned(a, size, 8);
}

void *arena_alloc_aligned(Arena *a, size_t size, size_t align) {
    size_t aligned = (a->offset + align - 1) & ~(align - 1);
    if (aligned + size > a->capacity) {
        SDL_Log("[arena] out of memory: need %zu, have %zu",
                size, a->capacity - aligned);
        return NULL;
    }
    void *ptr   = a->base + aligned;
    a->offset   = aligned + size;
    memset(ptr, 0, size);
    return ptr;
}

void arena_reset(Arena *a) {
    a->offset = 0;
}

void arena_destroy(Arena *a) {
    SDL_free(a->base);
    a->base     = NULL;
    a->offset   = 0;
    a->capacity = 0;
}
