#pragma once
#include "def.h"

typedef struct {
    real32 x, y;
} Vec2;

typedef struct {
    int32 x, y;
} IVec2;

Vec2 vec2(real32 x, real32 y);
IVec2 ivec2(int32 x, int32 y);
