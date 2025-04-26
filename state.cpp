#include "state.h"
#include "editor.h"
#include "filesystem.h"
#include <unistd.h>

void state_init(AppState *state) {
    state->mode = MODE_BROWSE;
    state->current_dir = getcwd(NULL, 0); // Получаем текущую директорию
    state->files = NULL;
    state->file_count = 0;
    state->selected_index = 0;
    state->edit_file = NULL;
    state->editor_state = NULL;
    state_load_files(state);
}

void state_free(AppState *state) {
    if (state->current_dir) free(state->current_dir);
    if (state->edit_file) free(state->edit_file);
    if (state->editor_state) {
        editor_free(state->editor_state);
        free(state->editor_state);
    }
    for (size_t i = 0; i < state->file_count; i++) {
        free(state->files[i].name);
    }
    if (state->files) free(state->files);
}

void state_set_current_dir(AppState *state, const char *path) {
    if (state->current_dir) free(state->current_dir);
    state->current_dir = strdup(path);
    state_load_files(state);
    state->selected_index = 0;
}

void state_load_files(AppState *state) {
    // Очищаем старый список файлов
    for (size_t i = 0; i < state->file_count; i++) {
        free(state->files[i].name);
    }
    if (state->files) free(state->files);
    state->files = NULL;
    state->file_count = 0;

    // Загружаем новый список файлов
    fs_get_files(state->current_dir, &state->files, &state->file_count);
}

void state_select_index(AppState *state, int index) {
    if (index >= 0 && index < (int)state->file_count) {
        state->selected_index = index;
    }
}

void state_set_edit_file(AppState *state, const char *filename) {
    if (state->edit_file) free(state->edit_file);
    state->edit_file = filename ? strdup(filename) : NULL;
    // Очищаем состояние редактора при смене файла
    if (state->editor_state) {
        editor_free(state->editor_state);
        free(state->editor_state);
        state->editor_state = NULL;
    }
}