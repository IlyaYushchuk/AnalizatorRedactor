#include "state_manager.h"
#include "UI.h"

int main() {
    if (!init_ui()) {
        return 1;
    }

    ProgramState state = init_state();
    render_ui(state);
    while (state.running) {
        int key = get_user_input();
        ProgramState new_state = handle_input(state, key);
        if (new_state.current_dir != state.current_dir ||
            new_state.files != state.files ||
            new_state.selected_idx != state.selected_idx ||
            new_state.selected_files != state.selected_files ||
            new_state.mode != state.mode ||
            new_state.status != state.status ||
            new_state.filter_pattern != state.filter_pattern) {
            state = new_state;
            render_ui(state);
        } else {
            state = new_state;
        }
    }

    cleanup_ui();
    return 0;
}