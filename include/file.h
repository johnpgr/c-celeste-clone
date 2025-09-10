#pragma once
#include "def.h"
#include "arena.h"

uint64 file_get_timestamp(const char* path);
bool file_exists(const char* path);
usize file_get_size(const char* path);
char* read_entire_file(Arena* arena, const char* path);
void write_file(const char* path, const char* data, usize size);
bool copy_file(Arena* arena, const char* src_path, const char* dst_path);