#pragma once
#include "def.h"

typedef struct {
    uint8* memory;
    usize size;
    usize offset;
} Arena;

Arena create_arena(usize size);
void arena_cleanup(Arena* arena);
void* arena_alloc(Arena* arena, usize size);
void arena_reset(Arena* arena);
usize arena_get_used(Arena* arena);
usize arena_get_remaining(Arena* arena);
