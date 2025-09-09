#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "arena.c"
#include "assets.c"
#include "math.c"
#include "game.c"
#include "gl_renderer.c"
#include "log.c"
#include "ogg.c"

#include "window.h"
#include "audio.h"

inline void update_fps_counter(Game* game, uint64 current_time) {
    game->frame_count++;
    if(current_time - game->fps_timer_start >= NANOS_PER_SEC) {
        game->current_fps = (real64)game->frame_count * NANOS_PER_SEC
            / (current_time - game->fps_timer_start);
        game->frame_count = 0;
        game->fps_timer_start = current_time;
    }
}

int main(int argc, [[maybe_unused]] char* argv[argc + 1]) {
    Game game = game_init();

    window_init(&game, WORLD_WIDTH * 4, WORLD_HEIGHT * 4);
    window_set_vsync(false);
    audio_init(&game);
    gl_init(&game);

    static uint8 background_ogg_source[] = {
        #embed "assets/sounds/Background.ogg"
    };
    usize background_ogg_size = sizeof(background_ogg_source);

    AudioSource* background_ogg = load_ogg_static_from_memory(
        &game,
        background_ogg_source,
        background_ogg_size,
        false
    );
    if (!background_ogg) {
        debug_print("ERROR: Failed to load background_ogg\n");
        return -1;
    }

    /* background_ogg->volume = 0.5f; */
    /* game_play_audio_source(background_ogg); */

    static uint8 explosion_ogg_source[] = {
        #embed "assets/sounds/Explosion.ogg"
    };
    usize explosion_ogg_size = sizeof(explosion_ogg_source);

    AudioSource* explosion_ogg = load_ogg_static_from_memory(
        &game,
        explosion_ogg_source,
        explosion_ogg_size,
        false
    );
    if (!explosion_ogg) {
        debug_print("ERROR: Failed to load explosion_ogg\n");
        return -1;
    }
    explosion_ogg->volume = 0.3f;

    const uint64 NANOS_PER_UPDATE = NANOS_PER_SEC / game.fps;
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

        window_poll_events();
        
        if (window_get_key_state(WINDOW_KEY_ESCAPE)) {
            break;
        }

        // TODO: Improve this key handling
        static bool space_was_pressed = false;
        static bool f_was_pressed = false;
        static bool m_was_pressed = false;
        bool space_is_pressed = window_get_key_state(WINDOW_KEY_SPACE);
        bool f_is_pressed = window_get_key_state(WINDOW_KEY_F);
        bool m_is_pressed = window_get_key_state(WINDOW_KEY_M);
        
        if (space_is_pressed && !space_was_pressed) {
            game_play_audio_source(explosion_ogg);
        }
        space_was_pressed = space_is_pressed;

        if (f_is_pressed && !f_was_pressed) {
            game.fps_cap = !game.fps_cap;
            debug_print("Frameskipping %s\n", game.fps_cap ? "Enabled" : "Disabled");
        }
        f_was_pressed = f_is_pressed;

        if (m_is_pressed && !m_was_pressed) {
            static real32 saved_volume = 1.0f;
            if (game.audio_volume > 0.0f) {
                saved_volume = game.audio_volume;
                game.audio_volume = 0.0f;
            } else {
                game.audio_volume = saved_volume;
            }
        }
        m_was_pressed = m_is_pressed;

        while (accumulator >= NANOS_PER_UPDATE) {
            game_generate_audio(&game);
            audio_update_buffer(&game);
            
            if (game.fps_cap) {
                update_fps_counter(&game, current_time);
                game_update_and_render(&game);
                window_present();
            } 
            
            accumulator -= NANOS_PER_UPDATE;
        }
        
        if (!game.fps_cap) {
            update_fps_counter(&game, current_time);
            game_update_and_render(&game);
            window_present();
        }
    }

    game_cleanup(&game);
    audio_cleanup();
    window_cleanup();
    gl_cleanup();

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
