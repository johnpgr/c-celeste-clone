#include <windows.h>
#include <dsound.h>
#include "platform_audio.h"

#define NUM_BUFFERS 3

typedef struct {
    int16* data;
    usize capacity;
    usize write_pos;
    usize read_pos;
    usize available;
    CRITICAL_SECTION mutex;
} RingBuffer;

static struct {
    AudioState* audio_state;
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
} win32_audio;

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
    AudioState* audio_state = (AudioState*)param;

    while (!win32_audio.should_stop) {
        DWORD wait_ms = (DWORD)(1000 / FPS);
        WaitForSingleObject(win32_audio.audio_event, wait_ms);
        
        if (!win32_audio.initialized || !audio_state) {
            continue;
        }

        DWORD play_pos = 0, write_pos_ignored = 0;
        HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(
            win32_audio.secondary_buffer, &play_pos, &write_pos_ignored);
        if (FAILED(hr)) {
            continue;
        }

        const uint32 buffer_size = win32_audio.buffer_size;
        const uint32 block_align = win32_audio.block_align;
        const uint32 safety_bytes = win32_audio.safety_bytes;

        uint32 target_write_pos = (play_pos + safety_bytes) % buffer_size;
        uint32 running_write_pos = win32_audio.running_write_pos;

        uint32 bytes_to_write = (target_write_pos + buffer_size - running_write_pos) % buffer_size;
        // Align to frame boundary
        bytes_to_write -= bytes_to_write % block_align;

        if (bytes_to_write == 0) {
            continue;
        }

        LPVOID audio_ptr1 = nullptr, audio_ptr2 = nullptr;
        DWORD audio_bytes1 = 0, audio_bytes2 = 0;

        hr = IDirectSoundBuffer_Lock(
            win32_audio.secondary_buffer,
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
                &win32_audio.ring_buffer,
                audio_data,
                samples_in_region1
            );

            if (samples_read < samples_in_region1) {
                memset(audio_data + samples_read, 0,
                       (samples_in_region1 - samples_read) * sizeof(int16));
            }

            if (win32_audio.audio_state->volume != 1.0f && samples_read > 0) {
                for (uint32 i = 0; i < samples_read; i++) {
                    audio_data[i] = (int16)((real32)audio_data[i] * win32_audio.audio_state->volume);
                }
            }
        }

        if (audio_ptr2 && audio_bytes2 > 0) {
            int16* audio_data = (int16*)audio_ptr2;
            uint32 samples_in_region2 = audio_bytes2 / sizeof(int16);

            usize samples_read = ring_buffer_read(
                &win32_audio.ring_buffer,
                audio_data,
                samples_in_region2
            );

            if (samples_read < samples_in_region2) {
                memset(audio_data + samples_read, 0,
                       (samples_in_region2 - samples_read) * sizeof(int16));
            }

            if (win32_audio.audio_state->volume != 1.0f && samples_read > 0) {
                for (uint32 i = 0; i < samples_read; i++) {
                    audio_data[i] = (int16)((real32)audio_data[i] * win32_audio.audio_state->volume);
                }
            }
        }

        IDirectSoundBuffer_Unlock(
            win32_audio.secondary_buffer,
            audio_ptr1, audio_bytes1,
            audio_ptr2, audio_bytes2
        );

        win32_audio.running_write_pos =
            (running_write_pos + bytes_to_write) % buffer_size;
    }
    
    return 0;
}

void platform_audio_init(AudioState* audio_state) {
    memset(&win32_audio, 0, sizeof(win32_audio));
    win32_audio.audio_state = audio_state;

    usize ring_buffer_capacity = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS;
    ring_buffer_init(&win32_audio.ring_buffer, ring_buffer_capacity);
    
    HRESULT hr = DirectSoundCreate(nullptr, &win32_audio.dsound, nullptr);
    if (FAILED(hr)) {
        debug_print("Error: Could not create DirectSound object (hr: 0x%08X)\n", (uint32)hr);
        return;
    }
    
    HWND hwnd = GetDesktopWindow();
    hr = IDirectSound_SetCooperativeLevel(win32_audio.dsound, hwnd, DSSCL_PRIORITY);
    if (FAILED(hr)) {
        debug_print("Error: Could not set DirectSound cooperative level (hr: 0x%08X)\n", (uint32)hr);
        IDirectSound_Release(win32_audio.dsound);
        return;
    }
    
    DSBUFFERDESC primary_desc = {};
    primary_desc.dwSize = sizeof(DSBUFFERDESC);
    primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    
    hr = IDirectSound_CreateSoundBuffer(
        win32_audio.dsound, 
        &primary_desc, 
        &win32_audio.primary_buffer, 
        nullptr
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not create primary buffer (hr: 0x%08X)\n", (uint32)hr);
        IDirectSound_Release(win32_audio.dsound);
        return;
    }
    
    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = (WORD)AUDIO_CHANNELS;
    wave_format.nSamplesPerSec = (DWORD)AUDIO_SAMPLE_RATE;
    wave_format.wBitsPerSample = sizeof(int16) * 8;
    wave_format.nBlockAlign = (WORD)(AUDIO_CHANNELS * sizeof(int16));
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
    
    hr = IDirectSoundBuffer_SetFormat(win32_audio.primary_buffer, &wave_format);
    if (FAILED(hr)) {
        debug_print("Error: Could not set primary buffer format (hr: 0x%08X)\n", (uint32)hr);
    }

    // Derived sizes for latency and buffer management
    win32_audio.block_align = wave_format.nBlockAlign;
    win32_audio.block_bytes = (uint32)((AUDIO_SAMPLE_RATE / FPS) * wave_format.nBlockAlign);
    win32_audio.buffer_size = win32_audio.block_bytes * NUM_BUFFERS;
    win32_audio.samples_per_buffer = win32_audio.buffer_size / sizeof(int16);
    
    DSBUFFERDESC secondary_desc = {};
    secondary_desc.dwSize = sizeof(DSBUFFERDESC);
    secondary_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    secondary_desc.dwBufferBytes = win32_audio.buffer_size;
    secondary_desc.lpwfxFormat = &wave_format;
    
    hr = IDirectSound_CreateSoundBuffer(
        win32_audio.dsound,
        &secondary_desc,
        &win32_audio.secondary_buffer,
        nullptr
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not create secondary buffer (hr: 0x%08X)\n", (uint32)hr);
        IDirectSoundBuffer_Release(win32_audio.primary_buffer);
        IDirectSound_Release(win32_audio.dsound);
        return;
    }
    
    LPVOID audio_ptr1, audio_ptr2;
    DWORD audio_bytes1, audio_bytes2;
    
    hr = IDirectSoundBuffer_Lock(
        win32_audio.secondary_buffer,
        0, win32_audio.buffer_size,
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
            win32_audio.secondary_buffer,
            audio_ptr1, audio_bytes1,
            audio_ptr2, audio_bytes2
        );
    }
    
    hr = IDirectSoundBuffer_Play(
        win32_audio.secondary_buffer, 
        0, 0, 
        DSBPLAY_LOOPING
    );
    
    if (FAILED(hr)) {
        debug_print("Error: Could not start playing secondary buffer (hr: 0x%08X)\n", (uint32)hr);
        IDirectSoundBuffer_Release(win32_audio.secondary_buffer);
        IDirectSoundBuffer_Release(win32_audio.primary_buffer);
        IDirectSound_Release(win32_audio.dsound);
        return;
    }

    // Initialize write position and target latency (one block ahead)
    {
        DWORD play_pos = 0, write_pos = 0;
        if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(
                win32_audio.secondary_buffer, &play_pos, &write_pos))) {
            win32_audio.running_write_pos = write_pos;
        } else {
            win32_audio.running_write_pos = 0;
        }
        win32_audio.safety_bytes = win32_audio.block_bytes; // ~1 frame (~16.7ms)
    }
    
    win32_audio.audio_event = CreateEvent(nullptr, false, false, nullptr);
    win32_audio.should_stop = false;
    
    win32_audio.audio_thread = CreateThread(
        nullptr, 0, 
        audio_thread_proc, 
        audio_state, 
        0, nullptr
    );
    
    if (!win32_audio.audio_thread) {
        debug_print("Error: Could not create audio thread\n");
        CloseHandle(win32_audio.audio_event);
        IDirectSoundBuffer_Release(win32_audio.secondary_buffer);
        IDirectSoundBuffer_Release(win32_audio.primary_buffer);
        IDirectSound_Release(win32_audio.dsound);
        return;
    }
    
    win32_audio.initialized = true;
    debug_print("DirectSound audio system initialized successfully\n");
}

void platform_audio_update_buffer() {
    if (!win32_audio.initialized || !win32_audio.audio_state) {
        return;
    }
    
    usize samples_written = ring_buffer_write(
        &win32_audio.ring_buffer,
        win32_audio.audio_state->audio,
        AUDIO_CAPACITY
    );
    
    if (samples_written < AUDIO_CAPACITY) {
        /* debug_print("Ring buffer is full, some audio data was dropped\n"); */
    }
    
    SetEvent(win32_audio.audio_event);
}

void platform_audio_set_volume(real32 volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    win32_audio.audio_state->volume = volume;
}

void platform_audio_cleanup(void) {
    win32_audio.should_stop = true;
    
    if (win32_audio.audio_thread) {
        SetEvent(win32_audio.audio_event);
        WaitForSingleObject(win32_audio.audio_thread, 1000);
        CloseHandle(win32_audio.audio_thread);
        win32_audio.audio_thread = nullptr;
    }
    
    if (win32_audio.audio_event) {
        CloseHandle(win32_audio.audio_event);
        win32_audio.audio_event = nullptr;
    }
    
    if (win32_audio.secondary_buffer) {
        IDirectSoundBuffer_Stop(win32_audio.secondary_buffer);
        IDirectSoundBuffer_Release(win32_audio.secondary_buffer);
        win32_audio.secondary_buffer = nullptr;
    }
    
    if (win32_audio.primary_buffer) {
        IDirectSoundBuffer_Release(win32_audio.primary_buffer);
        win32_audio.primary_buffer = nullptr;
    }
    
    if (win32_audio.dsound) {
        IDirectSound_Release(win32_audio.dsound);
        win32_audio.dsound = nullptr;
    }
    
    ring_buffer_destroy(&win32_audio.ring_buffer);
    win32_audio.initialized = false;
    
    debug_print("DirectSound audio system cleaned up\n");
}
