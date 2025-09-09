#include <windows.h>
#include <dsound.h>
#include "audio.h"
#include "game.h"

typedef struct {
    int16* data;
    usize capacity;
    usize write_pos;
    usize read_pos;
    usize available;
    CRITICAL_SECTION mutex;
} RingBuffer;

static struct {
    Game* game;
    LPDIRECTSOUND dsound;
    LPDIRECTSOUNDBUFFER primary_buffer;
    LPDIRECTSOUNDBUFFER secondary_buffer;
    RingBuffer ring_buffer;
    HANDLE audio_thread;
    HANDLE audio_event;
    bool initialized;
    bool should_stop;
    uint32 buffer_size;
    uint32 samples_per_buffer;

    // Streaming/latency control
    uint32 block_align;        // bytes per frame (all channels)
    uint32 block_bytes;        // one "tick" worth in bytes (1/fps seconds)
    uint32 safety_bytes;       // how far ahead of the play cursor we keep filled
    uint32 running_write_pos;  // next byte offset to write into secondary buffer
} Win32Audio;

internal void ring_buffer_init(RingBuffer* rb, usize capacity) {
    rb->data = calloc(capacity, sizeof(int16));
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

internal usize ring_buffer_write(RingBuffer* rb, const int16* data, usize samples) {
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

internal usize ring_buffer_read(RingBuffer* rb, int16* data, usize samples) {
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

    while (!Win32Audio.should_stop) {
        DWORD wait_ms = (game && game->fps) ? (DWORD)(1000 / game->fps) : 16;
        WaitForSingleObject(Win32Audio.audio_event, wait_ms);
        
        if (!Win32Audio.initialized || !game) {
            continue;
        }

        DWORD play_pos = 0, write_pos_ignored = 0;
        HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(
            Win32Audio.secondary_buffer, &play_pos, &write_pos_ignored);
        if (FAILED(hr)) {
            continue;
        }

        const uint32 buffer_size = Win32Audio.buffer_size;
        const uint32 block_align = Win32Audio.block_align;
        const uint32 safety_bytes = Win32Audio.safety_bytes;

        uint32 target_write_pos = (play_pos + safety_bytes) % buffer_size;
        uint32 running_write_pos = Win32Audio.running_write_pos;

        uint32 bytes_to_write = (target_write_pos + buffer_size - running_write_pos) % buffer_size;
        // Align to frame boundary
        bytes_to_write -= bytes_to_write % block_align;

        if (bytes_to_write == 0) {
            continue;
        }

        LPVOID audio_ptr1 = nullptr, audio_ptr2 = nullptr;
        DWORD audio_bytes1 = 0, audio_bytes2 = 0;

        hr = IDirectSoundBuffer_Lock(
            Win32Audio.secondary_buffer,
            running_write_pos,
            bytes_to_write,
            &audio_ptr1, &audio_bytes1,
            &audio_ptr2, &audio_bytes2,
            0
        );
        if (FAILED(hr)) {
            continue;
        }

        if (audio_ptr1 && audio_bytes1 > 0) {
            int16* audio_data = (int16*)audio_ptr1;
            uint32 samples_in_region1 = audio_bytes1 / sizeof(int16);

            usize samples_read = ring_buffer_read(
                &Win32Audio.ring_buffer,
                audio_data,
                samples_in_region1
            );

            if (samples_read < samples_in_region1) {
                memset(audio_data + samples_read, 0,
                       (samples_in_region1 - samples_read) * sizeof(int16));
            }

            if (game->audio_volume != 1.0f && samples_read > 0) {
                for (uint32 i = 0; i < samples_read; i++) {
                    audio_data[i] = (int16)((real32)audio_data[i] * game->audio_volume);
                }
            }
        }

        if (audio_ptr2 && audio_bytes2 > 0) {
            int16* audio_data = (int16*)audio_ptr2;
            uint32 samples_in_region2 = audio_bytes2 / sizeof(int16);

            usize samples_read = ring_buffer_read(
                &Win32Audio.ring_buffer,
                audio_data,
                samples_in_region2
            );

            if (samples_read < samples_in_region2) {
                memset(audio_data + samples_read, 0,
                       (samples_in_region2 - samples_read) * sizeof(int16));
            }

            if (game->audio_volume != 1.0f && samples_read > 0) {
                for (uint32 i = 0; i < samples_read; i++) {
                    audio_data[i] = (int16)((real32)audio_data[i] * game->audio_volume);
                }
            }
        }

        IDirectSoundBuffer_Unlock(
            Win32Audio.secondary_buffer,
            audio_ptr1, audio_bytes1,
            audio_ptr2, audio_bytes2
        );

        Win32Audio.running_write_pos =
            (running_write_pos + bytes_to_write) % buffer_size;
    }
    
    return 0;
}

void audio_init(Game* game) {
    memset(&Win32Audio, 0, sizeof(Win32Audio));
    Win32Audio.game = game;
    game->audio_volume = 1.0f;
    
    usize ring_buffer_capacity = game->audio_sample_rate * game->audio_channels;
    ring_buffer_init(&Win32Audio.ring_buffer, ring_buffer_capacity);
    
    HRESULT hr = DirectSoundCreate(nullptr, &Win32Audio.dsound, nullptr);
    if (FAILED(hr)) {
        debug_print("Error: Could not create DirectSound object (hr: 0x%08X)\n", (uint32)hr);
        return;
    }
    
    HWND hwnd = GetDesktopWindow();
    hr = IDirectSound_SetCooperativeLevel(Win32Audio.dsound, hwnd, DSSCL_PRIORITY);
    if (FAILED(hr)) {
        debug_print("Error: Could not set DirectSound cooperative level (hr: 0x%08X)\n", (uint32)hr);
        IDirectSound_Release(Win32Audio.dsound);
        return;
    }
    
    DSBUFFERDESC primary_desc = {};
    primary_desc.dwSize = sizeof(DSBUFFERDESC);
    primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    
    hr = IDirectSound_CreateSoundBuffer(
        Win32Audio.dsound, 
        &primary_desc, 
        &Win32Audio.primary_buffer, 
        nullptr
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not create primary buffer (hr: 0x%08X)\n", (uint32)hr);
        IDirectSound_Release(Win32Audio.dsound);
        return;
    }
    
    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = (WORD)game->audio_channels;
    wave_format.nSamplesPerSec = (DWORD)game->audio_sample_rate;
    wave_format.wBitsPerSample = sizeof(int16) * 8;
    wave_format.nBlockAlign = (WORD)(game->audio_channels * sizeof(int16));
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
    
    hr = IDirectSoundBuffer_SetFormat(Win32Audio.primary_buffer, &wave_format);
    if (FAILED(hr)) {
        debug_print("Error: Could not set primary buffer format (hr: 0x%08X)\n", (uint32)hr);
    }

    // Derived sizes for latency and buffer management
    Win32Audio.block_align = wave_format.nBlockAlign;
    Win32Audio.block_bytes = (uint32)((game->audio_sample_rate / game->fps) * wave_format.nBlockAlign);
    Win32Audio.buffer_size = Win32Audio.block_bytes * NUM_BUFFERS;
    Win32Audio.samples_per_buffer = Win32Audio.buffer_size / sizeof(int16);
    
    DSBUFFERDESC secondary_desc = {};
    secondary_desc.dwSize = sizeof(DSBUFFERDESC);
    secondary_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    secondary_desc.dwBufferBytes = Win32Audio.buffer_size;
    secondary_desc.lpwfxFormat = &wave_format;
    
    hr = IDirectSound_CreateSoundBuffer(
        Win32Audio.dsound,
        &secondary_desc,
        &Win32Audio.secondary_buffer,
        nullptr
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not create secondary buffer (hr: 0x%08X)\n", (uint32)hr);
        IDirectSoundBuffer_Release(Win32Audio.primary_buffer);
        IDirectSound_Release(Win32Audio.dsound);
        return;
    }
    
    LPVOID audio_ptr1, audio_ptr2;
    DWORD audio_bytes1, audio_bytes2;
    
    hr = IDirectSoundBuffer_Lock(
        Win32Audio.secondary_buffer,
        0, Win32Audio.buffer_size,
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
            Win32Audio.secondary_buffer,
            audio_ptr1, audio_bytes1,
            audio_ptr2, audio_bytes2
        );
    }
    
    hr = IDirectSoundBuffer_Play(
        Win32Audio.secondary_buffer, 
        0, 0, 
        DSBPLAY_LOOPING
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not start playing secondary buffer (hr: 0x%08X)\n", (uint32)hr);
        IDirectSoundBuffer_Release(Win32Audio.secondary_buffer);
        IDirectSoundBuffer_Release(Win32Audio.primary_buffer);
        IDirectSound_Release(Win32Audio.dsound);
        return;
    }

    // Initialize write position and target latency (one block ahead)
    {
        DWORD play_pos = 0, write_pos = 0;
        if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(
                Win32Audio.secondary_buffer, &play_pos, &write_pos))) {
            Win32Audio.running_write_pos = write_pos;
        } else {
            Win32Audio.running_write_pos = 0;
        }
        Win32Audio.safety_bytes = Win32Audio.block_bytes; // ~1 frame (~16.7ms)
    }
    
    Win32Audio.audio_event = CreateEvent(nullptr, false, false, nullptr);
    Win32Audio.should_stop = false;
    
    Win32Audio.audio_thread = CreateThread(
        nullptr, 0, 
        audio_thread_proc, 
        game, 
        0, nullptr
    );
    
    if (!Win32Audio.audio_thread) {
        debug_print("Error: Could not create audio thread\n");
        CloseHandle(Win32Audio.audio_event);
        IDirectSoundBuffer_Release(Win32Audio.secondary_buffer);
        IDirectSoundBuffer_Release(Win32Audio.primary_buffer);
        IDirectSound_Release(Win32Audio.dsound);
        return;
    }
    
    Win32Audio.initialized = true;
    debug_print("DirectSound audio system initialized successfully\n");
}

void audio_update_buffer(Game* game) {
    if (!Win32Audio.initialized || !game->audio) {
        return;
    }
    
    usize samples_written = ring_buffer_write(
        &Win32Audio.ring_buffer,
        game->audio,
        AUDIO_CAPACITY
    );
    
    if (samples_written < AUDIO_CAPACITY) {
        /* debug_print("Ring buffer is full, some audio data was dropped\n"); */
    }
    
    SetEvent(Win32Audio.audio_event);
}

void audio_set_volume(real32 volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    Win32Audio.game->audio_volume = volume;
}

void audio_cleanup(void) {
    Win32Audio.should_stop = true;
    
    if (Win32Audio.audio_thread) {
        SetEvent(Win32Audio.audio_event);
        WaitForSingleObject(Win32Audio.audio_thread, 1000);
        CloseHandle(Win32Audio.audio_thread);
        Win32Audio.audio_thread = nullptr;
    }
    
    if (Win32Audio.audio_event) {
        CloseHandle(Win32Audio.audio_event);
        Win32Audio.audio_event = nullptr;
    }
    
    if (Win32Audio.secondary_buffer) {
        IDirectSoundBuffer_Stop(Win32Audio.secondary_buffer);
        IDirectSoundBuffer_Release(Win32Audio.secondary_buffer);
        Win32Audio.secondary_buffer = nullptr;
    }
    
    if (Win32Audio.primary_buffer) {
        IDirectSoundBuffer_Release(Win32Audio.primary_buffer);
        Win32Audio.primary_buffer = nullptr;
    }
    
    if (Win32Audio.dsound) {
        IDirectSound_Release(Win32Audio.dsound);
        Win32Audio.dsound = nullptr;
    }
    
    ring_buffer_destroy(&Win32Audio.ring_buffer);
    Win32Audio.initialized = false;
    
    debug_print("DirectSound audio system cleaned up\n");
}
