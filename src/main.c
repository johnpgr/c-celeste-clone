#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "window.h"
#include "audio.h"
#include "utils.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    Game game = game_init();

    window_init(&game);
    audio_init(&game);

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

        while (accumulator >= NANOS_PER_UPDATE) {
            game_update_and_render();
            window_present();

            accumulator -= NANOS_PER_UPDATE;

            frame_count++;
            if(current_time - fps_timer_start >= NANOS_PER_SEC) {
                double fps = (double)frame_count * NANOS_PER_SEC / (current_time - fps_timer_start);
                printf("FPS: %.2f\n", fps);
                frame_count = 0;
                fps_timer_start = current_time;
            }
        }
    }

    audio_cleanup();
    window_cleanup();
    return EXIT_SUCCESS;
}
