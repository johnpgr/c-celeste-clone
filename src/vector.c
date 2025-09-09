#include <math.h>
#include "def.h"
#include "vector.h"

Vec2 vec2(real32 x, real32 y) {
    return (Vec2) {
        .x = x,
        .y = y,
    };
}

IVec2 ivec2(int32 x, int32 y) {
    return (IVec2) {
        .x = x,
        .y = y,
    };
}
