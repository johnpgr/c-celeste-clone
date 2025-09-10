#define _CRT_SECURE_NO_WARNINGS
#include "../platform/renderer/gl_renderer.c"
#include "../external/glad.c"

#ifdef _WIN32
#include "../platform/window/win32_window.c"
#include "../platform/audio/win32_audio.c"
#include "../platform/file/win32_file.c"
#include "../platform/dynlib/win32_dynlib.c"
#elif defined(__linux__)
#include "../platform/window/linux_window.c"
#include "../platform/audio/linux_audio.c"
#include "../platform/file/linux_file.c"
#include "../platform/dynlib/linux_dynlib.c"
#elif defined(__APPLE__)
#include "../platform/window/osx_window.m"
#include "../platform/audio/osx_audio.c"
#include "../platform/file/osx_file.c"
#include "../platform/dynlib/osx_dynlib.c"
#endif

typedef void GameUpdateFn(GameState*, RendererState*, InputState*, AudioState*);
static GameUpdateFn* game_update_ptr;

static void game_update(
    GameState* game_state,
    RendererState* renderer_state,
    InputState* input_state,
    AudioState* audio_state
) {
    game_update_ptr(game_state, renderer_state, input_state, audio_state);
}

static void reload_game_dll(Arena* transient_storage) {
    static void* game_dll;
    static uint64 game_dll_timestamp;

    uint64 current_dll_timestamp = file_get_timestamp(DYNLIB("game"));

    if (current_dll_timestamp > game_dll_timestamp) {
        if (game_dll) {
            dynlib_close(game_dll);
            game_dll = nullptr;
            debug_print("Unloaded old game dynlib\n");
        }

        while (!copy_file(transient_storage, DYNLIB("game"), DYNLIB("game_load"))) {
            sleep_nanos(10 * NANOS_PER_MILLI);
        }

        game_dll = dynlib_open(DYNLIB("game_load"));
        assert(game_dll != nullptr && "Failed to load game dynlib");

        game_update_ptr = (GameUpdateFn*)dynlib_get_symbol(game_dll, "game_update");
        assert(game_update_ptr != nullptr && "Failed to load update_game function");
        game_dll_timestamp = current_dll_timestamp;
    }
}

static void print_arena_stats(Arena* permanent_storage, Arena* transient_storage) {
    debug_print("Arena statistics:\n");
    debug_print(
        "  Permanent: %.1f/%.1f KB used (%.1f%%, %.1f KB remaining)\n",
        arena_get_used(permanent_storage) / 1024.0f,
        permanent_storage->size / 1024.0f,
        (real32)arena_get_used(permanent_storage) / permanent_storage->size * 100.0f,
        arena_get_remaining(permanent_storage) / 1024.0f
    );
    debug_print(
        "  Transient: %.1f/%.1f KB used (%.1f%%, %.1f KB remaining)\n",
        arena_get_used(transient_storage) / 1024.0f,
        transient_storage->size / 1024.0f,
        (real32)arena_get_used(transient_storage) / transient_storage->size * 100.0f,
        arena_get_remaining(transient_storage) / 1024.0f
    );
}

int main(int argc, [[maybe_unused]] char* argv[argc + 1]) {
    debug_print("Initializing game...\n");
    debug_print("  Audio: %d Hz, %zu channels, %zu capacity\n", AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_CAPACITY);
    debug_print("  FPS: %d\n", FPS);
    debug_print("  Max audio sources: %d\n", MAX_AUDIO_SOURCES);

    Arena permanent_storage = create_arena(MB(64));
    Arena transient_storage = create_arena(MB(128));
    debug_print("  Permanent arena: %.1f KB initialized\n", permanent_storage.size / 1024.0f);
    debug_print("  Transient arena: %.1f KB initialized\n", transient_storage.size / 1024.0f);

    // TODO: Maybe check if these allocations succeed
    renderer_state = create_renderer_state(&permanent_storage);
    game_state = create_game_state(&permanent_storage);
    input_state = create_input_state(&permanent_storage);
    audio_state = create_audio_state(&permanent_storage);

    window_init(input_state, renderer_state);
    platform_audio_init(audio_state);
    renderer_init(input_state, renderer_state);
    renderer_set_vsync(false);

    static uint8 background_ogg_source[] = {
        #embed "assets/sounds/Background.ogg"
    };
    usize background_ogg_size = sizeof(background_ogg_source);

    AudioSource* background_ogg = create_audio_source_static_memory(
        &permanent_storage,
        audio_state,
        background_ogg_source,
        background_ogg_size,
        false
    );
    if (!background_ogg) {
        debug_print("ERROR: Failed to load background_ogg\n");
        return -1;
    }

    background_ogg->volume = 0.5f;
    audio_source_play(background_ogg);

    static uint8 explosion_ogg_source[] = {
        #embed "assets/sounds/Explosion.ogg"
    };
    usize explosion_ogg_size = sizeof(explosion_ogg_source);

    AudioSource* explosion_ogg = create_audio_source_static_memory(
        &permanent_storage,
        audio_state,
        explosion_ogg_source,
        explosion_ogg_size,
        false
    );
    if (!explosion_ogg) {
        debug_print("ERROR: Failed to load explosion_ogg\n");
        return -1;
    }
    explosion_ogg->volume = 0.3f;

    const uint64 NANOS_PER_UPDATE = NANOS_PER_SEC / FPS;
    uint64 accumulator = 0;
    uint64 last_time = current_time_nanos();

    window_show();

    // TODO: Separate render thread from main thread 
    // (for resizing, moving the window without stopping the render)
    // Audio thread is already separate
    while (!window_should_close()) {
        uint64 current_time = current_time_nanos();
        uint64 delta_time = current_time - last_time;
        last_time = current_time;
        accumulator += delta_time;

        reload_game_dll(&transient_storage);

        window_poll_events();

        while (accumulator >= NANOS_PER_UPDATE) {
            platform_audio_update_buffer();
            
            if (game_state->fps_cap) {
                game_update(game_state, renderer_state, input_state, audio_state);
                renderer_render();
                window_present();
            } 
            
            accumulator -= NANOS_PER_UPDATE;
        }
        
        if (!game_state->fps_cap) {
            game_update(game_state, renderer_state, input_state, audio_state);
            renderer_render();
            window_present();
        }

        arena_reset(&transient_storage);
    }

    platform_audio_cleanup();
    window_cleanup();
    renderer_cleanup();
    arena_cleanup(&transient_storage);
    arena_cleanup(&permanent_storage);

    return EXIT_SUCCESS;
}

#ifdef _WIN32
int WINAPI WinMain(
   [[maybe_unused]] HINSTANCE hInstance,
   [[maybe_unused]] HINSTANCE hPrevInstance,
   [[maybe_unused]] LPSTR lpCmdLine,
   [[maybe_unused]] int nShowCmd
) {
    return main(0, nullptr);
}
#endif
