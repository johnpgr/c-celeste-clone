#include "file.h"
#include <windows.h>

uint64 file_get_timestamp(const char* path) {
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    FILETIME write_time;
    if (!GetFileTime(file, nullptr, nullptr, &write_time)) {
        CloseHandle(file);
        return 0;
    }
    
    CloseHandle(file);
    
    // Convert FILETIME to 64-bit timestamp
    ULARGE_INTEGER timestamp;
    timestamp.LowPart = write_time.dwLowDateTime;
    timestamp.HighPart = write_time.dwHighDateTime;
    
    return timestamp.QuadPart;
}

bool file_exists(const char* path) {
    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES &&
            !(attributes & FILE_ATTRIBUTE_DIRECTORY));
}

usize file_get_size(const char* path) {
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        return 0;
    }
    
    CloseHandle(file);
    return (usize)size.QuadPart;
}

char* read_entire_file(Arena* arena, const char* path) {
    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) {
        CloseHandle(file);
        return nullptr;
    }
    
    // Allocate buffer with null terminator
    char* buffer = (char*)arena_alloc(arena, (usize)file_size.QuadPart + 1);
    if (!buffer) {
        CloseHandle(file);
        return nullptr;
    }
    
    DWORD bytes_read;
    if (!ReadFile(file, buffer, (DWORD)file_size.QuadPart, &bytes_read, nullptr) || 
            bytes_read != file_size.QuadPart) {
        CloseHandle(file);
        return nullptr;
    }
    
    buffer[file_size.QuadPart] = '\0'; // Null terminate
    CloseHandle(file);
    
    return buffer;
}

void write_file(const char* path, const char* data, usize size) {
    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    
    DWORD bytes_written;
    WriteFile(file, data, (DWORD)size, &bytes_written, nullptr);
    CloseHandle(file);
}

bool copy_file(Arena* arena, const char* src_path, const char* dst_path) {
    // Read source file
    char* data = read_entire_file(arena, src_path);
    if (!data) {
        return false;
    }
    
    // Get source file size
    usize size = file_get_size(src_path);
    if (size == 0) {
        return false;
    }
    
    // Write to destination
    HANDLE dst_file = CreateFileA(
        dst_path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (dst_file == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytes_written;
    bool success = WriteFile(dst_file, data, (DWORD)size, &bytes_written, nullptr) && 
                   bytes_written == size;
    
    CloseHandle(dst_file);
    return success;
}