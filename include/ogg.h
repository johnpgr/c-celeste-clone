#pragma once

#include "audio-source.h"
#include "game.h"

AudioSource* load_ogg_streaming(Game* game, const char* filename, int stream_buffer_frames, bool loop);
AudioSource* load_ogg_static_from_memory(Game* game, const uint8* data, usize data_size, bool loop);
AudioSource* load_ogg_static(Game* game, const char* filename, bool loop);

bool refill_stream_buffer(AudioSource* source);
void start_stream_source(AudioSource* source);
void close_stream_source(AudioSource* source);
