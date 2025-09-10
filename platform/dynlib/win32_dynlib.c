#include "dynlib.h"
#include <windows.h>

void* dynlib_open(const char* path) {
    HMODULE handle = LoadLibraryA(path);
    return (void*)handle;
}

void* dynlib_get_symbol(void* handle, const char* symbol) {
    if (!handle) {
        return NULL;
    }
    
    FARPROC proc = GetProcAddress((HMODULE)handle, symbol);
    return (void*)proc;
}

void dynlib_close(void* handle) {
    if (handle) {
        FreeLibrary((HMODULE)handle);
    }
}