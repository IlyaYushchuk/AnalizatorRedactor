#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Инициализация состояния редактора
void editor_init(EditorState *editor) {
    editor->lines = NULL;
    editor->line_count = 0;
    editor->capacity = 0;
    editor->cursor_x = 0;
    editor->cursor_y = 0;
    editor->scroll_y = 0;
}

// Освобождение памяти
void editor_free(EditorState *editor) {
    for (size_t i = 0; i < editor->line_count; i++) {
        free(editor->lines[i]);
    }
    free(editor->lines);
    editor->lines = NULL;
    editor->line_count = 0;
    editor->capacity = 0;
}

// Загрузка файла в редактор
void editor_load_file(EditorState *editor, const char *filename) {
    printf("Loading file: %s\n", filename); // Отладочный вывод
    editor_free(editor); // Очищаем предыдущее содержимое

    FILE *file = fopen(filename, "r");
    if (!file) {
        // Если файл не существует, создаем пустой
        editor->lines = (char **)malloc(sizeof(char *));
        editor->lines[0] = strdup("");
        editor->line_count = 1;
        editor->capacity = 1;
        return;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    editor->capacity = 10;
    editor->lines = (char **)malloc(editor->capacity * sizeof(char *));

    while ((read = getline(&line, &len, file)) != -1) {
        if (editor->line_count >= editor->capacity) {
            editor->capacity *= 2;
            editor->lines = (char **)realloc(editor->lines, editor->capacity * sizeof(char *));
        }
        // Удаляем символ новой строки
        if (line[read - 1] == '\n') line[read - 1] = '\0';
        editor->lines[editor->line_count] = strdup(line);
        editor->line_count++;
    }

    free(line);
    fclose(file);

    if (editor->line_count == 0) {
        editor->lines[0] = strdup("");
        editor->line_count = 1;
        editor->capacity = 1;
    }
}

// Сохранение файла
static void editor_save_file(EditorState *editor, const char *filename) {
    printf("Saving file: %s\n", filename); // Отладочный вывод
    FILE *file = fopen(filename, "w");
    if (!file) return;

    for (size_t i = 0; i < editor->line_count; i++) {
        fprintf(file, "%s\n", editor->lines[i]);
    }

    fclose(file);
}

// Отрисовка редактора
void editor_draw(AppState *state) {
    // Инициализируем редактор, если он еще не создан
    if (!state->editor_state) {
        state->editor_state = (EditorState *)malloc(sizeof(EditorState));
        editor_init(state->editor_state);
        editor_load_file(state->editor_state, state->edit_file);
    }

    EditorState *editor = state->editor_state;
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Отрисовка строк
    for (int i = 0; i < max_y - 2 && i + editor->scroll_y < editor->line_count; i++) {
        mvprintw(i, 0, "%s", editor->lines[i + editor->scroll_y]);
        clrtoeol(); // Очистка остатка строки
    }

    // Отрисовка имени файла и горячих клавиш
    mvprintw(max_y - 1, 0, "Editing: %s | F2: Save | Esc: Exit | Arrows: Move", state->edit_file);
    move(editor->cursor_y - editor->scroll_y, editor->cursor_x);

    refresh();
}

// Обработка ввода
int editor_handle_input(AppState *state) {
    if (!state->editor_state) {
        state->editor_state = (EditorState *)malloc(sizeof(EditorState));
        editor_init(state->editor_state);
        editor_load_file(state->editor_state, state->edit_file);
    }

    EditorState *editor = state->editor_state;
    int ch = getch();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    switch (ch) {
        case 27: // Esc
            editor_free(editor);
            free(editor);
            state->editor_state = NULL;
            state->mode = MODE_BROWSE;
            state_set_edit_file(state, NULL);
            break;
        case KEY_F(2): // F2
            editor_save_file(editor, state->edit_file);
            break;
        case KEY_UP:
            if (editor->cursor_y > 0) {
                editor->cursor_y--;
                if (editor->cursor_y < editor->scroll_y) {
                    editor->scroll_y--;
                }
            }
            editor->cursor_x = editor->cursor_x < strlen(editor->lines[editor->cursor_y]) ?
                              editor->cursor_x : strlen(editor->lines[editor->cursor_y]);
            break;
        case KEY_DOWN:
            if (editor->cursor_y < editor->line_count - 1) {
                editor->cursor_y++;
                if (editor->cursor_y >= editor->scroll_y + max_y - 2) {
                    editor->scroll_y++;
                }
            }
            editor->cursor_x = editor->cursor_x < strlen(editor->lines[editor->cursor_y]) ?
                              editor->cursor_x : strlen(editor->lines[editor->cursor_y]);
            break;
        case KEY_LEFT:
            if (editor->cursor_x > 0) editor->cursor_x--;
            break;
        case KEY_RIGHT:
            if (editor->cursor_x < strlen(editor->lines[editor->cursor_y])) editor->cursor_x++;
            break;
        case KEY_BACKSPACE:
            if (editor->cursor_x > 0) {
                char *line = editor->lines[editor->cursor_y];
                memmove(line + editor->cursor_x - 1, line + editor->cursor_x, strlen(line) - editor->cursor_x + 1);
                editor->cursor_x--;
            } else if (editor->cursor_y > 0) {
                // Объединяем строки
                char *current_line = editor->lines[editor->cursor_y];
                editor->cursor_y--;
                editor->cursor_x = strlen(editor->lines[editor->cursor_y]);
                size_t new_len = strlen(editor->lines[editor->cursor_y]) + strlen(current_line) + 1;
                editor->lines[editor->cursor_y] = (char *)realloc(editor->lines[editor->cursor_y], new_len);
                strcat(editor->lines[editor->cursor_y], current_line);
                free(current_line);
                memmove(&editor->lines[editor->cursor_y + 1], &editor->lines[editor->cursor_y + 2],
                        (editor->line_count - editor->cursor_y - 2) * sizeof(char *));
                editor->line_count--;
            }
            break;
        default:
            if (ch >= 32 && ch <= 126) { // Печатные символы
                char *line = editor->lines[editor->cursor_y];
                size_t len = strlen(line);
                line = (char *)realloc(line, len + 2);
                memmove(line + editor->cursor_x + 1, line + editor->cursor_x, len - editor->cursor_x + 1);
                line[editor->cursor_x] = ch;
                editor->cursor_x++;
                editor->lines[editor->cursor_y] = line;
            }
            break;
    }

    return 1; // Продолжить
}