#include <windows.h>
#include <dsound.h>
#include <math.h>
#include "audio.h"
#include "game.h"

static struct {
    Game* game;
    LPDIRECTSOUND8 dsound;
    LPDIRECTSOUNDBUFFER primary_buffer;
    LPDIRECTSOUNDBUFFER secondary_buffer;
    DWORD buffer_size;
    DWORD write_cursor;
    double phase; // Track phase continuously
    HANDLE audio_thread;
    HANDLE stop_event;
    bool running;
} global_audio_state;

/**
 * @brief Audio thread that continuously fills the DirectSound buffer
 */
static DWORD WINAPI audio_thread_proc(LPVOID param) {
    Game* game = (Game*)param;
    if (!game) return 0;

    const double tone_hz = 440.0;
    const double phase_increment = 2.0 * PI * tone_hz / game->audio_sample_rate;
    const DWORD frames_per_update = game->audio_sample_rate / game->fps;
    const DWORD bytes_per_update = frames_per_update * game->audio_channels * sizeof(i16);
    
    while (global_audio_state.running && 
           WaitForSingleObject(global_audio_state.stop_event, 16) == WAIT_TIMEOUT) {
        
        DWORD play_cursor, write_cursor;
        HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(global_audio_state.secondary_buffer, 
                                                          &play_cursor, &write_cursor);
        if (FAILED(hr)) continue;

        // Calculate how much we can write
        DWORD bytes_to_write;
        if (global_audio_state.write_cursor <= write_cursor) {
            bytes_to_write = write_cursor - global_audio_state.write_cursor;
        } else {
            bytes_to_write = global_audio_state.buffer_size - global_audio_state.write_cursor + write_cursor;
        }

        // Only write if we have enough space
        if (bytes_to_write >= bytes_per_update) {
            LPVOID buffer_ptr1, buffer_ptr2;
            DWORD buffer_bytes1, buffer_bytes2;
            
            hr = IDirectSoundBuffer_Lock(global_audio_state.secondary_buffer,
                                       global_audio_state.write_cursor,
                                       bytes_per_update,
                                       &buffer_ptr1, &buffer_bytes1,
                                       &buffer_ptr2, &buffer_bytes2,
                                       0);
            
            if (SUCCEEDED(hr)) {
                // Fill first buffer segment
                i16* audio_data = (i16*)buffer_ptr1;
                DWORD frames_in_segment1 = buffer_bytes1 / (game->audio_channels * sizeof(i16));
                
                for (DWORD i = 0; i < frames_in_segment1; i++) {
                    float sample = sinf((float)global_audio_state.phase) * 0.1f;
                    i16 sample_i16 = (i16)(sample * 32767.0f);
                    
                    // Fill both channels (stereo)
                    audio_data[i * game->audio_channels + 0] = sample_i16; // Left
                    audio_data[i * game->audio_channels + 1] = sample_i16; // Right
                    
                    // Increment phase and wrap to prevent precision loss
                    global_audio_state.phase += phase_increment;
                    if (global_audio_state.phase >= 2.0 * PI) {
                        global_audio_state.phase -= 2.0 * PI;
                    }
                }
                
                // Fill second buffer segment if it exists (buffer wraparound)
                if (buffer_ptr2) {
                    audio_data = (i16*)buffer_ptr2;
                    DWORD frames_in_segment2 = buffer_bytes2 / (game->audio_channels * sizeof(i16));
                    
                    for (DWORD i = 0; i < frames_in_segment2; i++) {
                        float sample = sinf((float)global_audio_state.phase) * 0.1f;
                        i16 sample_i16 = (i16)(sample * 32767.0f);
                        
                        audio_data[i * game->audio_channels + 0] = sample_i16; // Left
                        audio_data[i * game->audio_channels + 1] = sample_i16; // Right
                        
                        global_audio_state.phase += phase_increment;
                        if (global_audio_state.phase >= 2.0 * PI) {
                            global_audio_state.phase -= 2.0 * PI;
                        }
                    }
                }
                
                IDirectSoundBuffer_Unlock(global_audio_state.secondary_buffer,
                                        buffer_ptr1, buffer_bytes1,
                                        buffer_ptr2, buffer_bytes2);
                
                // Update write cursor
                global_audio_state.write_cursor = (global_audio_state.write_cursor + bytes_per_update) % global_audio_state.buffer_size;
            }
        }
    }
    
    return 0;
}

void audio_init(Game* game) {
    ZeroMemory(&global_audio_state, sizeof(global_audio_state));
    global_audio_state.game = game;
    global_audio_state.phase = 0.0;
    global_audio_state.running = true;

    // Create DirectSound object
    HRESULT hr = DirectSoundCreate8(NULL, &global_audio_state.dsound, NULL);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: Could not create DirectSound object (HRESULT: 0x%08lX)\n", hr);
        return;
    }

    // Set cooperative level (assuming you have a window handle available)
    // For this example, we'll use the desktop window
    HWND hwnd = GetDesktopWindow();
    hr = IDirectSound8_SetCooperativeLevel(global_audio_state.dsound, hwnd, DSSCL_PRIORITY);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: Could not set DirectSound cooperative level (HRESULT: 0x%08lX)\n", hr);
        IDirectSound8_Release(global_audio_state.dsound);
        return;
    }

    // Set up the primary buffer format
    DSBUFFERDESC primary_desc;
    ZeroMemory(&primary_desc, sizeof(primary_desc));
    primary_desc.dwSize = sizeof(primary_desc);
    primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    hr = IDirectSound8_CreateSoundBuffer(global_audio_state.dsound, &primary_desc, 
                                        &global_audio_state.primary_buffer, NULL);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: Could not create primary sound buffer (HRESULT: 0x%08lX)\n", hr);
        IDirectSound8_Release(global_audio_state.dsound);
        return;
    }

    // Set primary buffer format
    WAVEFORMATEX wave_format;
    ZeroMemory(&wave_format, sizeof(wave_format));
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = (WORD)game->audio_channels;
    wave_format.nSamplesPerSec = game->audio_sample_rate;
    wave_format.wBitsPerSample = sizeof(i16) * 8;
    wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;

    hr = IDirectSoundBuffer_SetFormat(global_audio_state.primary_buffer, &wave_format);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: Could not set primary buffer format (HRESULT: 0x%08lX)\n", hr);
    }

    // Create secondary buffer (the one we actually write to)
    global_audio_state.buffer_size = game->audio_sample_rate * game->audio_channels * sizeof(i16); // 1 second buffer

    DSBUFFERDESC secondary_desc;
    ZeroMemory(&secondary_desc, sizeof(secondary_desc));
    secondary_desc.dwSize = sizeof(secondary_desc);
    secondary_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    secondary_desc.dwBufferBytes = global_audio_state.buffer_size;
    secondary_desc.lpwfxFormat = &wave_format;

    hr = IDirectSound8_CreateSoundBuffer(global_audio_state.dsound, &secondary_desc,
                                        &global_audio_state.secondary_buffer, NULL);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: Could not create secondary sound buffer (HRESULT: 0x%08lX)\n", hr);
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        IDirectSound8_Release(global_audio_state.dsound);
        return;
    }

    // Clear the buffer initially
    LPVOID buffer_ptr1, buffer_ptr2;
    DWORD buffer_bytes1, buffer_bytes2;
    hr = IDirectSoundBuffer_Lock(global_audio_state.secondary_buffer, 0, global_audio_state.buffer_size,
                               &buffer_ptr1, &buffer_bytes1, &buffer_ptr2, &buffer_bytes2, 0);
    if (SUCCEEDED(hr)) {
        ZeroMemory(buffer_ptr1, buffer_bytes1);
        if (buffer_ptr2) {
            ZeroMemory(buffer_ptr2, buffer_bytes2);
        }
        IDirectSoundBuffer_Unlock(global_audio_state.secondary_buffer,
                                buffer_ptr1, buffer_bytes1, buffer_ptr2, buffer_bytes2);
    }

    // Create stop event for thread synchronization
    global_audio_state.stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!global_audio_state.stop_event) {
        fprintf(stderr, "Error: Could not create audio stop event\n");
        IDirectSoundBuffer_Release(global_audio_state.secondary_buffer);
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        IDirectSound8_Release(global_audio_state.dsound);
        return;
    }

    // Start playing the buffer (looping)
    hr = IDirectSoundBuffer_Play(global_audio_state.secondary_buffer, 0, 0, DSBPLAY_LOOPING);
    if (FAILED(hr)) {
        fprintf(stderr, "Error: Could not start playing sound buffer (HRESULT: 0x%08lX)\n", hr);
    }

    // Create and start the audio thread
    global_audio_state.audio_thread = CreateThread(NULL, 0, audio_thread_proc, game, 0, NULL);
    if (!global_audio_state.audio_thread) {
        fprintf(stderr, "Error: Could not create audio thread\n");
        CloseHandle(global_audio_state.stop_event);
        IDirectSoundBuffer_Release(global_audio_state.secondary_buffer);
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        IDirectSound8_Release(global_audio_state.dsound);
        return;
    }
}

void audio_cleanup(void) {
    if (global_audio_state.running) {
        global_audio_state.running = false;
        
        // Signal the thread to stop
        if (global_audio_state.stop_event) {
            SetEvent(global_audio_state.stop_event);
        }
        
        // Wait for thread to finish
        if (global_audio_state.audio_thread) {
            WaitForSingleObject(global_audio_state.audio_thread, 5000); // 5 second timeout
            CloseHandle(global_audio_state.audio_thread);
            global_audio_state.audio_thread = NULL;
        }
        
        // Close event handle
        if (global_audio_state.stop_event) {
            CloseHandle(global_audio_state.stop_event);
            global_audio_state.stop_event = NULL;
        }
    }

    // Stop playback
    if (global_audio_state.secondary_buffer) {
        IDirectSoundBuffer_Stop(global_audio_state.secondary_buffer);
        IDirectSoundBuffer_Release(global_audio_state.secondary_buffer);
        global_audio_state.secondary_buffer = NULL;
    }

    if (global_audio_state.primary_buffer) {
        IDirectSoundBuffer_Release(global_audio_state.primary_buffer);
        global_audio_state.primary_buffer = NULL;
    }

    if (global_audio_state.dsound) {
        IDirectSound8_Release(global_audio_state.dsound);
        global_audio_state.dsound = NULL;
    }
}

