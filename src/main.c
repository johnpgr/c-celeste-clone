#include "game.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int main(int argc, [[maybe_unused]] char* argv[argc+1]) {
    Game game = game_init();

    window_init(&game);
    /* window_set_vsync(true); */

    struct timeval last_time;
    gettimeofday(&last_time, NULL);
    int frames = 0;
    double fps_display_interval = 1.0;
    
    while (!window_should_close()) {
        window_poll_events();
        
        // Check input state
        if (window_get_key_state(WINDOW_KEY_ESCAPE)) {
            break;
        }
        
        game_update();
        window_present();
        frames++;

        struct timeval current_time;
        gettimeofday(&current_time, NULL);

        double elapsed_time = (current_time.tv_sec - last_time.tv_sec) + 
            (current_time.tv_usec - last_time.tv_usec) / 1000000.0;

        if (elapsed_time >= fps_display_interval) {
            double fps = frames / elapsed_time;
            printf("FPS: %.2f\n", fps);

            last_time = current_time;
            frames = 0;
        }
    }

    window_cleanup();
    return EXIT_SUCCESS;
}
