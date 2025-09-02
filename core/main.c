#include <stdlib.h>
#include "game.h"
#include "window.h"
#include "audio.h"
#include "utils.h"
#include "ogg.h"

inline void update_fps_counter(Game* game, u64 current_time) {
    game->frame_count++;
    if(current_time - game->fps_timer_start >= NANOS_PER_SEC) {
        game->current_fps = (double)game->frame_count * NANOS_PER_SEC / (current_time - game->fps_timer_start);
        game->frame_count = 0;
        game->fps_timer_start = current_time;
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    Game game = game_init();

    window_init(&game);
    audio_init(&game);

    constexpr u8 background_audio[] = {
        #embed "assets/Background.ogg"
    };
    constexpr usize background_audio_size = sizeof(background_audio);

    AudioSource* background_ogg = load_ogg_static_from_memory(
        &game,
        background_audio,
        background_audio_size,
        false
    );
    if (!background_ogg) {
        debug_print("ERROR: Failed to load background_ogg\n");
        return -1;
    }

    background_ogg->volume = 0.5f;
    game_play_audio_source(background_ogg);

    constexpr u8 explosion_audio[] = {
        #embed "assets/Explosion.ogg"
    };
    constexpr usize explosion_audio_size = sizeof(explosion_audio);

    AudioSource* explosion_ogg = load_ogg_static_from_memory(
        &game,
        explosion_audio,
        explosion_audio_size,
        false
    );
    explosion_ogg->volume = 0.3f;

    const u64 NANOS_PER_UPDATE = NANOS_PER_SEC / game.fps;
    u64 accumulator = 0;
    u64 last_time = current_time_nanos();


    // TODO: Separate render thread from main thread 
    // (for resizing, moving the window without stopping the render)
    // Audio thread is already separate
    while (!window_should_close()) {
        u64 current_time = current_time_nanos();
        u64 delta_time = current_time - last_time;
        last_time = current_time;
        accumulator += delta_time;

        window_poll_events();
        
        if (window_get_key_state(WINDOW_KEY_ESCAPE)) {
            break;
        }

        // TODO: Improve this key handling
        static bool space_was_pressed = false;
        static bool f_was_pressed = false;
        bool space_is_pressed = window_get_key_state(WINDOW_KEY_SPACE);
        bool f_is_pressed = window_get_key_state(WINDOW_KEY_F);
        
        if (space_is_pressed && !space_was_pressed) {
            game_play_audio_source(explosion_ogg);
        }
        space_was_pressed = space_is_pressed;

        if (f_is_pressed && !f_was_pressed) {
            game.frame_skip = !game.frame_skip;
        }
        f_was_pressed = f_is_pressed;

        while (accumulator >= NANOS_PER_UPDATE) {
            game_generate_audio(&game);
            audio_update_buffer(&game);
            
            if (game.frame_skip) {
                update_fps_counter(&game, current_time);
                game_update_and_render(&game);
            }
            
            accumulator -= NANOS_PER_UPDATE;
        }
        
        if (!game.frame_skip) {
            update_fps_counter(&game, current_time);
            game_update_and_render(&game);
        }

        window_present();
    }

    game_cleanup(&game);
    audio_cleanup();
    window_cleanup();

    return EXIT_SUCCESS;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(0, nullptr);
}
#endif
