#include "def.h"
#include "game.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, [[maybe_unused]] char* argv[argc+1]) {
    Game game = game_init();

    for (usize x = 0; x < game.display_width; x++) {
        for (usize y = 0; y < game.display_height; y++) {
            game_display_pixel(&game, x, y, 0xff000080);
        }
    }

    window_init(&game);
    window_run();

    printf("Window closed.\n");
    return EXIT_SUCCESS;
}
