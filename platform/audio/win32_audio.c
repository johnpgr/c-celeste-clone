#include <windows.h>
#include <dsound.h>
#include "audio.h"
#include "game.h"

typedef struct {
    i16* data;
    usize capacity;
    usize write_pos;
    usize read_pos;
    usize available;
    CRITICAL_SECTION mutex;
} RingBuffer;

internal struct {
    Game* game;
    LPDIRECTSOUND dsound;
    LPDIRECTSOUNDBUFFER primary_buffer;
    LPDIRECTSOUNDBUFFER secondary_buffer;
    RingBuffer ring_buffer;
    HANDLE audio_thread;
    HANDLE audio_event;
    bool initialized;
    bool should_stop;
    u32 buffer_size;
    u32 samples_per_buffer;
} global_audio_state;

internal void ring_buffer_init(RingBuffer* rb, usize capacity) {
    rb->data = calloc(capacity, sizeof(i16));
    rb->capacity = capacity;
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->available = 0;
    InitializeCriticalSection(&rb->mutex);
}

internal void ring_buffer_destroy(RingBuffer* rb) {
    DeleteCriticalSection(&rb->mutex);
    free(rb->data);
    rb->data = nullptr;
}

internal usize ring_buffer_write(RingBuffer* rb, const i16* data, usize samples) {
    EnterCriticalSection(&rb->mutex);
    
    usize space_available = rb->capacity - rb->available;
    usize samples_to_write = (samples < space_available) ? samples : space_available;
    
    for (usize i = 0; i < samples_to_write; i++) {
        rb->data[rb->write_pos] = data[i];
        rb->write_pos = (rb->write_pos + 1) % rb->capacity;
    }
    
    rb->available += samples_to_write;
    
    LeaveCriticalSection(&rb->mutex);
    return samples_to_write;
}

internal usize ring_buffer_read(RingBuffer* rb, i16* data, usize samples) {
    EnterCriticalSection(&rb->mutex);
    
    usize samples_to_read = (samples < rb->available) ? samples : rb->available;
    
    for (usize i = 0; i < samples_to_read; i++) {
        data[i] = rb->data[rb->read_pos];
        rb->read_pos = (rb->read_pos + 1) % rb->capacity;
    }
    
    rb->available -= samples_to_read;
    
    LeaveCriticalSection(&rb->mutex);
    return samples_to_read;
}

internal DWORD WINAPI audio_thread_proc(LPVOID param) {
    Game* game = (Game*)param;
    DWORD last_play_pos = 0;
    
    while (!global_audio_state.should_stop) {
        WaitForSingleObject(global_audio_state.audio_event, 1000.0f / game->fps);
        
        if (!global_audio_state.initialized || !game) {
            continue;
        }
        
        DWORD play_pos, write_pos;
        HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(
            global_audio_state.secondary_buffer, &play_pos, &write_pos);
        
        if (FAILED(hr)) {
            continue;
        }
        
        DWORD bytes_to_write;
        if (play_pos >= last_play_pos) {
            bytes_to_write = play_pos - last_play_pos;
        } else {
            bytes_to_write = (global_audio_state.buffer_size - last_play_pos) + play_pos;
        }
        
        if (bytes_to_write == 0) {
            continue;
        }
        
        u32 samples_to_write = bytes_to_write / sizeof(i16);
        
        LPVOID audio_ptr1, audio_ptr2;
        DWORD audio_bytes1, audio_bytes2;
        
        hr = IDirectSoundBuffer_Lock(
            global_audio_state.secondary_buffer,
            last_play_pos,
            bytes_to_write,
            &audio_ptr1, &audio_bytes1,
            &audio_ptr2, &audio_bytes2,
            0
        );
        
        if (FAILED(hr)) {
            continue;
        }
        
        if (audio_ptr1 && audio_bytes1 > 0) {
            i16* audio_data = (i16*)audio_ptr1;
            u32 samples_in_region1 = audio_bytes1 / sizeof(i16);
            
            usize samples_read = ring_buffer_read(
                &global_audio_state.ring_buffer, 
                audio_data, 
                samples_in_region1
            );
            
            if (samples_read < samples_in_region1) {
                memset(audio_data + samples_read, 0, 
                       (samples_in_region1 - samples_read) * sizeof(i16));
            }
            
            if (game->audio_volume != 1.0f) {
                for (u32 i = 0; i < samples_read; i++) {
                    audio_data[i] = (i16)((float)audio_data[i] * game->audio_volume);
                }
            }
        }
        
        if (audio_ptr2 && audio_bytes2 > 0) {
            i16* audio_data = (i16*)audio_ptr2;
            u32 samples_in_region2 = audio_bytes2 / sizeof(i16);
            
            usize samples_read = ring_buffer_read(
                &global_audio_state.ring_buffer, 
                audio_data, 
                samples_in_region2
            );
            
            if (samples_read < samples_in_region2) {
                memset(audio_data + samples_read, 0, 
                       (samples_in_region2 - samples_read) * sizeof(i16));
            }
            
            if (game->audio_volume != 1.0f) {
                for (u32 i = 0; i < samples_read; i++) {
                    audio_data[i] = (i16)((float)audio_data[i] * game->audio_volume);
                }
            }
        }
        
        IDirectSoundBuffer_Unlock(
            global_audio_state.secondary_buffer,
            audio_ptr1, audio_bytes1,
            audio_ptr2, audio_bytes2
        );
        
        last_play_pos = play_pos;
    }
    
    return 0;
}

void audio_init(Game* game) {
    memset(&global_audio_state, 0, sizeof(global_audio_state));
    global_audio_state.game = game;
    game->audio_volume = 1.0f;
    
    usize ring_buffer_capacity = game->audio_sample_rate * game->audio_channels;
    ring_buffer_init(&global_audio_state.ring_buffer, ring_buffer_capacity);
    
    HRESULT hr = DirectSoundCreate(nullptr, &global_audio_state.dsound, nullptr);
    if (FAILED(hr)) {
        debug_print("Error: Could not create DirectSound object (hr: 0x%08X)\n", (u32)hr);
        return;
    }
    
    HWND hwnd = GetDesktopWindow();
    hr = IDirectSound_SetCooperativeLevel(global_audio_state.dsound, hwnd, DSSCL_PRIORITY);
    if (FAILED(hr)) {
        debug_print("Error: Could not set DirectSound cooperative level (hr: 0x%08X)\n", (u32)hr);
        IDirectSound_Release(global_audio_state.dsound);
        return;
    }
    
    DSBUFFERDESC primary_desc = {0};
    primary_desc.dwSize = sizeof(DSBUFFERDESC);
    primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    
    hr = IDirectSound_CreateSoundBuffer(
        global_audio_state.dsound, 
        &primary_desc, 
        &global_audio_state.primary_buffer, 
        nullptr
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not create primary buffer (hr: 0x%08X)\n", (u32)hr);
        IDirectSound_Release(global_audio_state.dsound);
        return;
    }
    
    WAVEFORMATEX wave_format = {0};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = (WORD)game->audio_channels;
    wave_format.nSamplesPerSec = (DWORD)game->audio_sample_rate;
    wave_format.wBitsPerSample = sizeof(i16) * 8;
    wave_format.nBlockAlign = (WORD)(game->audio_channels * sizeof(i16));
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
    
    hr = IDirectSoundBuffer_SetFormat(global_audio_state.primary_buffer, &wave_format);
    if (FAILED(hr)) {
        debug_print("Error: Could not set primary buffer format (hr: 0x%08X)\n", (u32)hr);
    }
    
    global_audio_state.buffer_size = game->audio_channels * 
        (game->audio_sample_rate / game->fps) * sizeof(i16) * NUM_BUFFERS;
    global_audio_state.samples_per_buffer = global_audio_state.buffer_size / sizeof(i16);
    
    DSBUFFERDESC secondary_desc = {0};
    secondary_desc.dwSize = sizeof(DSBUFFERDESC);
    secondary_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    secondary_desc.dwBufferBytes = global_audio_state.buffer_size;
    secondary_desc.lpwfxFormat = &wave_format;
    
    hr = IDirectSound_CreateSoundBuffer(
        global_audio_state.dsound,
        &secondary_desc,
        &global_audio_state.secondary_buffer,
        nullptr
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not create secondary buffer (hr: 0x%08X)\n", (u32)hr);
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        IDirectSound_Release(global_audio_state.dsound);
        return;
    }
    
    LPVOID audio_ptr1, audio_ptr2;
    DWORD audio_bytes1, audio_bytes2;
    
    hr = IDirectSoundBuffer_Lock(
        global_audio_state.secondary_buffer,
        0, global_audio_state.buffer_size,
        &audio_ptr1, &audio_bytes1,
        &audio_ptr2, &audio_bytes2,
        DSBLOCK_ENTIREBUFFER
    );
    
    if (SUCCEEDED(hr)) {
        memset(audio_ptr1, 0, audio_bytes1);
        if (audio_ptr2) {
            memset(audio_ptr2, 0, audio_bytes2);
        }
        
        IDirectSoundBuffer_Unlock(
            global_audio_state.secondary_buffer,
            audio_ptr1, audio_bytes1,
            audio_ptr2, audio_bytes2
        );
    }
    
    hr = IDirectSoundBuffer_Play(
        global_audio_state.secondary_buffer, 
        0, 0, 
        DSBPLAY_LOOPING
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not start playing secondary buffer (hr: 0x%08X)\n", (u32)hr);
        IDirectSoundBuffer_Release(global_audio_state.secondary_buffer);
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        IDirectSound_Release(global_audio_state.dsound);
        return;
    }
    
    global_audio_state.audio_event = CreateEvent(nullptr, false, false, nullptr);
    global_audio_state.should_stop = false;
    
    global_audio_state.audio_thread = CreateThread(
        nullptr, 0, 
        audio_thread_proc, 
        game, 
        0, nullptr
    );
    
    if (!global_audio_state.audio_thread) {
        debug_print("Error: Could not create audio thread\n");
        CloseHandle(global_audio_state.audio_event);
        IDirectSoundBuffer_Release(global_audio_state.secondary_buffer);
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        IDirectSound_Release(global_audio_state.dsound);
        return;
    }
    
    global_audio_state.initialized = true;
    debug_print("DirectSound audio system initialized successfully\n");
}

void audio_update_buffer(Game* game) {
    if (!global_audio_state.initialized || !game->audio) {
        return;
    }
    
    usize samples_written = ring_buffer_write(
        &global_audio_state.ring_buffer,
        game->audio,
        AUDIO_CAPACITY
    );
    
    if (samples_written < AUDIO_CAPACITY) {
        /* debug_print("Ring buffer is full, some audio data was dropped\n"); */
    }
    
    SetEvent(global_audio_state.audio_event);
}

void audio_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    global_audio_state.game->audio_volume = volume;
}

void audio_cleanup(void) {
    global_audio_state.should_stop = true;
    
    if (global_audio_state.audio_thread) {
        SetEvent(global_audio_state.audio_event);
        WaitForSingleObject(global_audio_state.audio_thread, 1000);
        CloseHandle(global_audio_state.audio_thread);
        global_audio_state.audio_thread = nullptr;
    }
    
    if (global_audio_state.audio_event) {
        CloseHandle(global_audio_state.audio_event);
        global_audio_state.audio_event = nullptr;
    }
    
    if (global_audio_state.secondary_buffer) {
        IDirectSoundBuffer_Stop(global_audio_state.secondary_buffer);
        IDirectSoundBuffer_Release(global_audio_state.secondary_buffer);
        global_audio_state.secondary_buffer = nullptr;
    }
    
    if (global_audio_state.primary_buffer) {
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        global_audio_state.primary_buffer = nullptr;
    }
    
    if (global_audio_state.dsound) {
        IDirectSound_Release(global_audio_state.dsound);
        global_audio_state.dsound = nullptr;
    }
    
    ring_buffer_destroy(&global_audio_state.ring_buffer);
    global_audio_state.initialized = false;
    
    debug_print("DirectSound audio system cleaned up\n");
}
