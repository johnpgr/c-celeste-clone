#pragma once
#include "def.h"
#include "arena.h"

#ifdef _WIN32
#define DYNLIB(name) name ".dll"
#elif defined(__linux__)
#define DYNLIB(name) "lib" name ".so"
#elif defined(__APPLE__)
#define DYNLIB(name) "lib" name ".dylib"
#endif

void* dynlib_open(const char* path);
void* dynlib_get_symbol(void* handle, const char* symbol);
void dynlib_close(void* handle);
