#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm> // Для std::min

void editor_init(EditorState *editor) {
    if (!editor) {
        fprintf(stderr, "Error: editor_init called with NULL editor\n");
        return;
    }
    editor->lines = NULL;
    editor->line_count = 0;
    editor->capacity = 0;
    editor->cursor_x = 0;
    editor->cursor_y = 0;
    editor->scroll_y = 0;
    // printf("Editor initialized\n");
}

void editor_free(EditorState *editor) {
    if (!editor) {
        fprintf(stderr, "Error: editor_free called with NULL editor\n");
        return;
    }
    for (size_t i = 0; i < editor->line_count; ++i) {
        free(editor->lines[i]);
        editor->lines[i] = NULL;
    }
    free(editor->lines);
    editor->lines = NULL;
    editor->line_count = 0;
    editor->capacity = 0;
    // printf("Editor freed\n");
}

void editor_load_file(EditorState *editor, const char *filename) {
    if (!editor) {
        fprintf(stderr, "Error: editor_load_file called with NULL editor\n");
        return;
    }
    if (!filename) {
        fprintf(stderr, "Error: editor_load_file called with NULL filename\n");
        return;
    }
    // printf("Loading file: %s\n", filename);
    editor_free(editor);

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        editor->lines = (char **)malloc(sizeof(char *));
        if (!editor->lines) {
            fprintf(stderr, "Failed to allocate memory for lines\n");
            return;
        }
        editor->lines[0] = strdup("");
        if (!editor->lines[0]) {
            fprintf(stderr, "Failed to duplicate empty line\n");
            free(editor->lines);
            editor->lines = NULL;
            return;
        }
        editor->line_count = 1;
        editor->capacity = 1;
        // printf("Created empty file with 1 line\n");
        return;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    editor->capacity = 10;
    editor->lines = (char **)malloc(editor->capacity * sizeof(char *));
    if (!editor->lines) {
        fprintf(stderr, "Failed to allocate memory for lines\n");
        fclose(file);
        return;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        if (editor->line_count >= editor->capacity) {
            editor->capacity *= 2;
            char **new_lines = (char **)realloc(editor->lines, editor->capacity * sizeof(char *));
            if (!new_lines) {
                fprintf(stderr, "Failed to reallocate memory for lines\n");
                free(line);
                fclose(file);
                editor_free(editor);
                return;
            }
            editor->lines = new_lines;
        }
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        editor->lines[editor->line_count] = strdup(line);
        if (!editor->lines[editor->line_count]) {
            fprintf(stderr, "Failed to duplicate line\n");
            free(line);
            fclose(file);
            editor_free(editor);
            return;
        }
        editor->line_count++;
    }

    free(line);
    fclose(file);

    if (editor->line_count == 0) {
        editor->lines = (char **)malloc(sizeof(char *));
        if (!editor->lines) {
            fprintf(stderr, "Failed to allocate memory for empty lines\n");
            return;
        }
        editor->lines[0] = strdup("");
        if (!editor->lines[0]) {
            fprintf(stderr, "Failed to duplicate empty line\n");
            free(editor->lines);
            editor->lines = NULL;
            return;
        }
        editor->line_count = 1;
        editor->capacity = 1;
    }
    // printf("File loaded: %zu lines\n", editor->line_count);
}

static void editor_save_file(EditorState *editor, const char *filename) {
    if (!editor || !filename) {
        fprintf(stderr, "Error: editor_save_file called with NULL editor or filename\n");
        return;
    }
    // printf("Saving file: %s\n", filename);
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Failed to save file: %s\n", filename);
        return;
    }

    for (size_t i = 0; i < editor->line_count; ++i) {
        if (editor->lines[i]) {
            fprintf(file, "%s\n", editor->lines[i]);
        } else {
            fprintf(file, "\n");
        }
    }

    fclose(file);
    // printf("File saved\n");
}

void editor_draw(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: editor_draw called with NULL state\n");
        return;
    }
    if (!state->edit_file) {
        fprintf(stderr, "Error: edit_file is NULL\n");
        state->mode = MODE_BROWSE;
        return;
    }
    if (!state->editor_state) {
        // printf("Initializing editor_state\n");
        state->editor_state = (EditorState *)malloc(sizeof(EditorState));
        if (!state->editor_state) {
            fprintf(stderr, "Failed to allocate memory for editor_state\n");
            state->mode = MODE_BROWSE;
            return;
        }
        editor_init(state->editor_state);
        editor_load_file(state->editor_state, state->edit_file);
    }

    EditorState *editor = state->editor_state;
    if (!editor->lines || editor->line_count == 0) {
        fprintf(stderr, "Error: editor lines not initialized\n");
        state->mode = MODE_BROWSE;
        return;
    }

    unsigned int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    clear();

    for (size_t i = 0; i < static_cast<size_t>(max_y - 2) && editor->scroll_y + i < editor->line_count; ++i) {
        if (editor->lines[editor->scroll_y + i]) {
            mvprintw(i, 0, "%s", editor->lines[editor->scroll_y + i]);
        }
        clrtoeol();
    }

    mvprintw(max_y - 1, 0, "Editing: %s | F2: Save | Esc: Exit | Arrows: Move | Enter: New Line | Backspace: Delete", state->edit_file);

    if (editor->cursor_y >= editor->scroll_y && editor->cursor_y < editor->scroll_y + static_cast<size_t>(max_y - 2)) {
        move(editor->cursor_y - editor->scroll_y, editor->cursor_x);
    }

    refresh();
    // Удалено: printf("Editor drawn: cursor at (%u, %u), scroll_y=%u\n", editor->cursor_x, editor->cursor_y, editor->scroll_y);
}

int editor_handle_input(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: editor_handle_input called with NULL state\n");
        return 0;
    }
    if (!state->editor_state) {
        fprintf(stderr, "Error: editor_state is NULL\n");
        state->mode = MODE_BROWSE;
        return 0;
    }
    EditorState *editor = state->editor_state;
    if (!editor->lines || editor->line_count == 0) {
        fprintf(stderr, "Error: editor lines not initialized\n");
        state->mode = MODE_BROWSE;
        return 0;
    }

    int ch = getch();
    unsigned int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    switch (ch) {
        case 27: // Esc
            editor_free(editor);
            free(editor);
            state->editor_state = NULL;
            state->mode = MODE_BROWSE;
            state_set_edit_file(state, NULL);
            // printf("Exiting editor\n");
            break;

        case KEY_F(2): // Сохранение
            if (state->edit_file) {
                editor_save_file(editor, state->edit_file);
            }
            break;

        case KEY_UP:
            if (editor->cursor_y > 0) {
                editor->cursor_y--;
                if (editor->cursor_y < editor->scroll_y) {
                    editor->scroll_y--;
                }
                editor->cursor_x = std::min(editor->cursor_x,
                                            static_cast<unsigned int>(strlen(editor->lines[editor->cursor_y])));
            }
            break;

        case KEY_DOWN:
            if (editor->cursor_y + 1 < editor->line_count) {
                editor->cursor_y++;
                if (editor->cursor_y >= editor->scroll_y + static_cast<size_t>(max_y - 2)) {
                    editor->scroll_y++;
                }
                editor->cursor_x = std::min(editor->cursor_x,
                                            static_cast<unsigned int>(strlen(editor->lines[editor->cursor_y])));
            }
            break;

        case KEY_LEFT:
            if (editor->cursor_x > 0) {
                editor->cursor_x--;
            }
            break;

        case KEY_RIGHT:
            if (editor->cursor_x < static_cast<unsigned int>(strlen(editor->lines[editor->cursor_y]))) {
                editor->cursor_x++;
            }
            break;

        case KEY_BACKSPACE: // Backspace
            if (editor->cursor_x > 0) {
                // Удаляем символ слева от курсора
                char *line = editor->lines[editor->cursor_y];
                size_t len = strlen(line);
                memmove(line + editor->cursor_x - 1, line + editor->cursor_x, len - editor->cursor_x + 1);
                editor->cursor_x--;
            } else if (editor->cursor_y > 0) {
                // Объединяем с предыдущей строкой
                char *current_line = editor->lines[editor->cursor_y];
                editor->cursor_y--;
                editor->cursor_x = strlen(editor->lines[editor->cursor_y]);
                size_t prev_len = strlen(editor->lines[editor->cursor_y]);
                size_t curr_len = strlen(current_line);
                char *new_line = (char *)realloc(editor->lines[editor->cursor_y], prev_len + curr_len + 1);
                if (!new_line) {
                    fprintf(stderr, "Failed to reallocate memory for line\n");
                    return 0;
                }
                strcat(new_line, current_line);
                editor->lines[editor->cursor_y] = new_line;
                free(current_line);
                memmove(&editor->lines[editor->cursor_y + 1], &editor->lines[editor->cursor_y + 2],
                        (editor->line_count - editor->cursor_y - 2) * sizeof(char *));
                editor->line_count--;
                if (editor->cursor_y < editor->scroll_y) {
                    editor->scroll_y--;
                }
            }
            break;

        case '\n': { // Enter (новая строка)
            if (editor->line_count >= editor->capacity) {
                editor->capacity = editor->capacity ? editor->capacity * 2 : 10;
                char **new_lines = (char **)realloc(editor->lines, editor->capacity * sizeof(char *));
                if (!new_lines) {
                    fprintf(stderr, "Failed to reallocate memory for lines\n");
                    return 0;
                }
                editor->lines = new_lines;
            }
            char *current_line = editor->lines[editor->cursor_y];
            size_t remaining_len = strlen(current_line + editor->cursor_x) + 1;
            char *new_line = (char *)malloc(remaining_len);
            if (!new_line) {
                fprintf(stderr, "Failed to allocate memory for new line\n");
                return 0;
            }
            strcpy(new_line, current_line + editor->cursor_x);
            current_line[editor->cursor_x] = '\0';
            memmove(&editor->lines[editor->cursor_y + 2], &editor->lines[editor->cursor_y + 1],
                    (editor->line_count - editor->cursor_y - 1) * sizeof(char *));
            editor->lines[editor->cursor_y + 1] = new_line;
            editor->line_count++;
            editor->cursor_y++;
            editor->cursor_x = 0;
            if (editor->cursor_y >= editor->scroll_y + static_cast<size_t>(max_y - 2)) {
                editor->scroll_y++;
            }
            break;
        }

        default:
            if (ch >= 32 && ch <= 126) { // Печатные символы
                char *line = editor->lines[editor->cursor_y];
                size_t len = strlen(line);
                char *new_line = (char *)realloc(line, len + 2);
                if (!new_line) {
                    fprintf(stderr, "Failed to reallocate memory for line\n");
                    return 0;
                }
                memmove(new_line + editor->cursor_x + 1, new_line + editor->cursor_x, len - editor->cursor_x + 1);
                new_line[editor->cursor_x] = static_cast<char>(ch);
                editor->cursor_x++;
                editor->lines[editor->cursor_y] = new_line;
            }
            break;
    }

    return 1;
}   