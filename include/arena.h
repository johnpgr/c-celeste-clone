#pragma once
#include "def.h"

typedef struct {
    uint8* memory;
    usize size;
    usize offset;
} Arena;

// TODO: Use platform-specific allocation functions
Arena create_arena(usize size) {
    Arena arena = {
        .memory = (uint8*)malloc(size),
        .size = size,
        .offset = 0,
    };
    return arena;
}

void arena_cleanup(Arena *arena) {
    free(arena->memory);
}

void *arena_alloc(Arena *arena, usize size) {
    // Align to 8 bytes for better performance
    usize aligned_size = (size + 7) & ~7;
    
    if (arena->offset + aligned_size > arena->size) {
        debug_print("Error: Arena out of memory (requested: %.1f KB, available: %.1f KB)\n", 
                   aligned_size / 1024.0f, (arena->size - arena->offset) / 1024.0f);
        return nullptr;
    }
    
    void* ptr = arena->memory + arena->offset;
    arena->offset += aligned_size;
    
    return ptr;
}

void arena_reset(Arena* arena) {
    arena->offset = 0;
}

usize arena_get_used(Arena* arena) {
    return arena->offset;
}

usize arena_get_remaining(Arena* arena) {
    return arena->size - arena->offset;
}
