#include "state.h"
#include "filesystem.h"
#include "editor.h"
#include "analysis.h"
#include <unistd.h>
#include <strings.h> // Для strcasestr
#include <algorithm>

void state_init(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: state_init called with NULL state\n");
        return;
    }
    state->mode = MODE_BROWSE;
    state->current_dir = getcwd(NULL, 0);
    state->selected_index = 0;
    state->edit_file = NULL;
    state->editor_state = NULL;
    state->analysis_result = NULL;
    state->search_query = NULL;
    state->clipboard_path = NULL;
    state->clipboard_is_cut = false;
    state->sort_type = SORT_BY_NAME_ASC;
    memset(state->search_input, 0, sizeof(state->search_input));
    state->search_active = true;  
    state->scroll_y = 0;
    state_load_files(state);
}

void state_sort_files(AppState *state) {
    if (!state) return;

    auto &files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
    switch (state->sort_type) {
        case SORT_BY_NAME_ASC:
            std::sort(files.begin(), files.end(), [](const FileInfo &a, const FileInfo &b) {
                return strcmp(a.name, b.name) < 0;
            });
            break;
        case SORT_BY_NAME_DESC:
            std::sort(files.begin(), files.end(), [](const FileInfo &a, const FileInfo &b) {
                return strcmp(a.name, b.name) > 0;
            });
            break;
        case SORT_BY_SIZE_ASC:
            std::sort(files.begin(), files.end(), [](const FileInfo &a, const FileInfo &b) {
                return a.size < b.size;
            });
            break;
        case SORT_BY_SIZE_DESC:
            std::sort(files.begin(), files.end(), [](const FileInfo &a, const FileInfo &b) {
                return a.size > b.size;
            });
            break;
        case SORT_BY_DATE_ASC:
            std::sort(files.begin(), files.end(), [](const FileInfo &a, const FileInfo &b) {
                return a.mtime < b.mtime;
            });
            break;
        case SORT_BY_DATE_DESC:
            std::sort(files.begin(), files.end(), [](const FileInfo &a, const FileInfo &b) {
                return a.mtime > b.mtime;
            });
            break;
    }
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
    for (auto &file : state->files) {
        if (file.name) free(file.name);
    }
    state->files.clear();
    for (auto &file : state->filtered_files) {
        if (file.name) free(file.name);
    }
    state->filtered_files.clear();
    if (state->search_query) free(state->search_query);
    state->current_dir = NULL;
    state->edit_file = NULL;
    state->editor_state = NULL;
    state->analysis_result = NULL;
    state->search_query = NULL;
    if (state->clipboard_path) free(state->clipboard_path);
    state->clipboard_path = NULL;
    state->clipboard_is_cut = false;
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
    for (auto &file : state->files) {
        if (file.name) free(file.name);
    }
    state->files.clear();
    for (auto &file : state->filtered_files) {
        if (file.name) free(file.name);
    }
    state->filtered_files.clear();
    if (state->search_query) {
        free(state->search_query);
        state->search_query = NULL;
    }

    fs_get_files(state->current_dir, state->files);
    state_sort_files(state); // Добавляем сортировку
    state->scroll_y = 0;
}

void state_filter_files(AppState *state, const char *query) {
    if (!state) {
        fprintf(stderr, "Error: state_filter_files called with NULL state\n");
        return;
    }
    for (auto &file : state->filtered_files) {
        if (file.name) free(file.name);
    }
    state->filtered_files.clear();
    if (state->search_query) {
        free(state->search_query);
        state->search_query = NULL;
    }
    if (!query || strlen(query) == 0) {
        state->selected_index = state->files.size() > 0 ? std::min(state->selected_index, static_cast<long long>(state->files.size() - 1)) : 0;
        return;
    }
    state->search_query = strdup(query);
    if (!state->search_query) {
        fprintf(stderr, "Failed to allocate memory for search query\n");
        return;
    }
    printf("Starting search from directory: %s\n", state->current_dir);
    fs_search_recursive(state->current_dir, query, state->filtered_files);
    state_sort_files(state); // Добавляем сортировку
    state->selected_index = state->filtered_files.size() > 0 ? 0 : 0;
    state->scroll_y = 0;
}

void state_select_index(AppState *state, unsigned int index) {
    if (!state) {
        fprintf(stderr, "Error: state_select_index called with NULL state\n");
        return;
    }
    long long max_index = state->filtered_files.size() > 0 ? state->filtered_files.size() - 1 : state->files.size() - 1;
    state->selected_index = index <= static_cast<unsigned int>(max_index) ? index : max_index;
    if (state->selected_index < 0) state->selected_index = 0;
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
