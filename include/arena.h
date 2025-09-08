#pragma once
#include "def.h"

typedef struct {
    uint8* memory;
    usize size;
    usize offset;
    const char* name;
} Arena;

void* arena_alloc(Arena* arena, usize size);
void arena_reset(Arena* arena);
usize arena_get_used(Arena* arena);
usize arena_get_remaining(Arena* arena);
