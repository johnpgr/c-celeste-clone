#include "arena.h"

void* arena_alloc(Arena* arena, usize size) {
    // Align to 8 bytes for better performance
    usize aligned_size = (size + 7) & ~7;
    
    if (arena->offset + aligned_size > arena->size) {
        debug_print("Error: %s arena out of memory (requested: %.1f KB, available: %.1f KB)\n", 
                   arena->name, aligned_size / 1024.0f, (arena->size - arena->offset) / 1024.0f);
        return nullptr;
    }
    
    void* ptr = arena->memory + arena->offset;
    arena->offset += aligned_size;
    
#if DEBUG_ARENA_ALLOCATIONS
    debug_print("%s arena alloc: %.1f KB at offset %.1f KB (remaining: %.1f KB)\n", 
               arena->name, aligned_size / 1024.0f, (arena->offset - aligned_size) / 1024.0f, 
               (arena->size - arena->offset) / 1024.0f);
#endif
    
    return ptr;
}

void arena_reset(Arena* arena) {
#if DEBUG_ARENA_RESETS
    debug_print("%s arena reset: freeing %.1f KB\n", arena->name, arena->offset / 1024.0f);
#endif
    arena->offset = 0;
}

usize arena_get_used(Arena* arena) {
    return arena->offset;
}

usize arena_get_remaining(Arena* arena) {
    return arena->size - arena->offset;
}
