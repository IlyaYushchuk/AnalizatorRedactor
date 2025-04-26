#include "state.h"
#include "ui.h"

int main() {
    AppState state;
    state_init(&state);
    ui_init();

    while (ui_handle_input(&state)) {
        ui_draw(&state);
    }

    ui_deinit();
    state_free(&state);
    return 0;
}