#pragma once
#include "def.h"

#define ARRAY(T, N) struct { \
    T data[N]; \
    usize count; \
    usize capacity; \
}

#define ARRAY_INIT(N) { \
    .data = {0}, \
    .count = 0, \
    .capacity = N \
}

#define create_array(N) ARRAY_INIT(N)

#define array_get(arr, index) \
    ({ \
        assert((index) < (arr).count && "Fixed array index out of bounds"); \
        (arr).data[index]; \
    })

#define array_set(arr, index, value) \
    do { \
        assert((index) < (arr).count && "Fixed array index out of bounds"); \
        (arr).data[index] = (value); \
    } while(0)

#define array_push(arr, value) \
    do { \
        assert((arr).count < (arr).capacity && "Fixed array is full"); \
        (arr).data[(arr).count++] = (value); \
    } while(0)

#define array_try_push(arr, value) \
    ({ \
        bool success = (arr).count < (arr).capacity; \
        if (success) { \
            (arr).data[(arr).count++] = (value); \
        } \
        success; \
    })

#define array_pop(arr) \
    ({ \
        assert((arr).count > 0 && "Cannot pop from empty fixed array"); \
        (arr).data[--(arr).count]; \
    })

#define array_last(arr) \
    ({ \
        assert((arr).count > 0 && "Fixed array is empty"); \
        (arr).data[(arr).count - 1]; \
    })

#define array_is_empty(arr) ((arr).count == 0)

#define array_is_full(arr) ((arr).count >= (arr).capacity)

#define array_clear(arr) ((arr).count = 0)

#define array_len(arr) ((arr).count)

#define array_cap(arr) ((arr).capacity)

#define array_insert(arr, index, value) \
    do { \
        assert((index) <= (arr).count && "Insert index out of bounds"); \
        assert((arr).count < (arr).capacity && "Fixed array is full"); \
        for (usize i = (arr).count; i > (index); i--) { \
            (arr).data[i] = (arr).data[i - 1]; \
        } \
        (arr).data[index] = (value); \
        (arr).count++; \
    } while(0)

#define array_remove(arr, index) \
    ({ \
        assert((index) < (arr).count && "Remove index out of bounds"); \
        typeof((arr).data[0]) removed = (arr).data[index]; \
        for (usize i = (index); i < (arr).count - 1; i++) { \
            (arr).data[i] = (arr).data[i + 1]; \
        } \
        (arr).count--; \
        removed; \
    })
