#pragma once
#include "def.h"
#include "arena.h"

void* dynlib_open(const char* path);
void* dynlib_get_symbol(void* handle, const char* symbol);
void dynlib_close(void* handle);