#include <stdlib.h>
#include "game.h"
#include "window.h"
#include "audio.h"
#include "utils.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    Game game = game_init();

    window_init(&game);
    audio_init(&game);

    constexpr u8 test_audio[] = {
        #embed "assets/test.ogg"
    };
    constexpr usize test_audio_size = sizeof(test_audio);

    AudioSource* test_ogg = game_load_ogg_static_from_memory(&game, test_audio, test_audio_size, false);

    const u64 NANOS_PER_UPDATE = NANOS_PER_SEC / game.fps;
    u64 accumulator = 0;
    u64 frame_count = 0;
    u64 last_time = current_time_nanos();
    u64 fps_timer_start = current_time_nanos();
    
    while (!window_should_close()) {
        u64 current_time = current_time_nanos();
        u64 delta_time = current_time - last_time;
        last_time = current_time;
        accumulator += delta_time;

        window_poll_events();
        
        // Check input state
        if (window_get_key_state(WINDOW_KEY_ESCAPE)) {
            break;
        }

        static bool space_was_pressed = false;
        bool space_is_pressed = window_get_key_state(WINDOW_KEY_SPACE);
        
        if (space_is_pressed && !space_was_pressed) {
            game_play_audio_source(test_ogg); // Trigger sound effect
        }
        space_was_pressed = space_is_pressed;

        while (accumulator >= NANOS_PER_UPDATE) {
            game_update_and_render(&game);
            game_generate_audio(&game);
            audio_update_buffer(&game);
            window_present();

            accumulator -= NANOS_PER_UPDATE;

            frame_count++;
            if(current_time - fps_timer_start >= NANOS_PER_SEC) {
                double fps = (double)frame_count * NANOS_PER_SEC /
                    (current_time - fps_timer_start);
                debug_print("FPS: %.2f\n", fps);
                frame_count = 0;
                fps_timer_start = current_time;
            }
        }
    }

    for (usize i = 0; i < game.max_audio_sources; i++) {
        game_free_audio_source(&game.loaded_audio[i]);
    }
    audio_cleanup();
    window_cleanup();
    return EXIT_SUCCESS;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(0, nullptr);
}
#endif
