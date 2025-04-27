#include "state.h"
#include "filesystem.h"
#include "editor.h"
#include "analysis.h"
#include <unistd.h>

void state_init(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: state_init called with NULL state\n");
        return;
    }
    state->mode = MODE_BROWSE;
    state->current_dir = getcwd(NULL, 0);
    state->files = NULL;
    state->file_count = 0;
    state->selected_index = 0;
    state->edit_file = NULL;
    state->editor_state = NULL;
    state->analysis_result = NULL;
    state_load_files(state);
}

void state_free(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: state_free called with NULL state\n");
        return;
    }
    if (state->current_dir) free(state->current_dir);
    if (state->edit_file) free(state->edit_file);
    if (state->editor_state) {
        editor_free(state->editor_state);
        free(state->editor_state);
    }
    if (state->analysis_result) {
        analysis_free(state->analysis_result);
        delete state->analysis_result;
    }
    for (long long i = 0; i < state->file_count; i++) {
        if (state->files[i].name) free(state->files[i].name);
    }
    if (state->files) free(state->files);
}

void state_set_current_dir(AppState *state, const char *path) {
    if (!state || !path) {
        fprintf(stderr, "Error: state_set_current_dir called with NULL state or path\n");
        return;
    }
    if (state->current_dir) free(state->current_dir);
    state->current_dir = strdup(path);
    if (!state->current_dir) {
        fprintf(stderr, "Error: failed to duplicate path\n");
        return;
    }
    state_load_files(state);
    state->selected_index = 0;
}

void state_load_files(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: state_load_files called with NULL state\n");
        return;
    }
    for (long long i = 0; i < state->file_count; i++) {
        if (state->files[i].name) free(state->files[i].name);
    }
    if (state->files) free(state->files);
    state->files = NULL;
    state->file_count = 0;

    fs_get_files(state->current_dir, &state->files, &state->file_count);
}

void state_select_index(AppState *state, unsigned int index) {
    if (!state) {
        fprintf(stderr, "Error: state_select_index called with NULL state\n");
        return;
    }
    if (index < state->file_count) {
        state->selected_index = index;
    }
}

void state_set_edit_file(AppState *state, const char *filename) {
    if (!state) {
        fprintf(stderr, "Error: state_set_edit_file called with NULL state\n");
        return;
    }
    if (state->edit_file) {
        free(state->edit_file);
        state->edit_file = NULL;
    }
    if (filename) {
        state->edit_file = strdup(filename);
        if (!state->edit_file) {
            fprintf(stderr, "Error: failed to duplicate filename\n");
            return;
        }
    }
    if (state->editor_state) {
        editor_free(state->editor_state);
        free(state->editor_state);
        state->editor_state = NULL;
    }
    printf("Set edit file: %s\n", filename ? filename : "NULL");
}