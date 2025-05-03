#include "handlers.h"
#include "ui.h"
#include "filesystem.h"
#include "editor.h"
#include "analysis.h"
#include "state.h"
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <algorithm>
#include <unistd.h>
#include <limits.h>

int browse_handle_input(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: browse_handle_input called with NULL state\n");
        return 0;
    }

    int ch = getch();
    MEVENT event;
    bool quit = false;
    switch (ch) {
        case 'q':
            quit = true;
            return 0;
        case '\n':
        {
            const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
            if (state->selected_index < static_cast<long long>(display_files.size())) {
                const FileInfo &file = display_files[state->selected_index];
                if (state->filtered_files.size() > 0 && !file.is_dir) {
                    // Для результатов поиска у нас полный путь в file.name
                    char *last_slash = strrchr(file.name, '/');
                    if (last_slash) {
                        char dir_path[PATH_MAX];
                        strncpy(dir_path, file.name, last_slash - file.name);
                        dir_path[last_slash - file.name] = '\0';

                        // Сохраняем текущую директорию
                        char original_dir[PATH_MAX];
                        strncpy(original_dir, state->current_dir, PATH_MAX);
                        original_dir[PATH_MAX - 1] = '\0';

                        // Переходим в директорию файла
                        state_set_current_dir(state, dir_path);

                        // Открываем файл
                        const char *filename = last_slash + 1;
                        printf("Attempting to open file from search: %s (from dir: %s)\n", filename, dir_path);
                        fs_open_file(state, filename);

                        // Возвращаемся в исходную директорию
                        state_set_current_dir(state, original_dir);
                    } else {
                        printf("Attempting to open file from search: %s\n", file.name);
                        fs_open_file(state, file.name);
                    }
                } else if (file.is_dir) {
                    fs_open_dir(state, file.name);
                } else {
                    printf("Attempting to open file: %s\n", file.name);
                    fs_open_file(state, file.name);
                }
            }
        }
        break;
        case 'f': // Активация поиска
            state->mode = MODE_SEARCH;
            state_filter_files(state, "");
            break;
        case KEY_F(3):
        {
            clear();
            mvprintw(0, 0, "Enter number of days for old files (default 180): ");
            refresh();
            echo();
            char input[32] = "";
            int y, x;
            getyx(stdscr, y, x);
            move(y, x);
            getnstr(input, sizeof(input) - 1);
            noecho();

            long days = 180;
            if (strlen(input) > 0) {
                char *endptr;
                days = strtol(input, &endptr, 10);
                if (*endptr != '\0' || days < 0) {
                    mvprintw(1, 0, "Invalid input, using default (180 days)");
                    refresh();
                    getch();
                    days = 180;
                }
            }

            time_t now = time(NULL);
            time_t old_threshold = now - days * 24 * 3600;
            analysis_perform(state, old_threshold);
        }
            break;
        case KEY_F(5): // Создание файла
        {
            clear();
            mvprintw(0, 0, "Enter new file name: ");
            refresh();
            echo();
            char filename[256] = "";
            getnstr(filename, sizeof(filename) - 1);
            noecho();
            if (strlen(filename) > 0) {
                fs_create_file(state, filename);
            }
        }
            break;
        case KEY_F(6): // Создание папки
        {
            clear();
            mvprintw(0, 0, "Enter new directory name: ");
            refresh();
            echo();
            char dirname[256] = "";
            getnstr(dirname, sizeof(dirname) - 1);
            noecho();
            if (strlen(dirname) > 0) {
                fs_create_dir(state, dirname);
            }
        }
            break;
        case KEY_F(7): // Переименование
        {
            const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
            if (state->selected_index < static_cast<long long>(display_files.size())) {
                clear();
                mvprintw(0, 0, "Enter new name for %s: ", display_files[state->selected_index].name);
                refresh();
                echo();
                char new_name[256] = "";
                getnstr(new_name, sizeof(new_name) - 1);
                noecho();
                if (strlen(new_name) > 0) {
                    fs_rename(state, display_files[state->selected_index].name, new_name);
                }
            }
        }
            break;
        case 3: // Ctrl+C (копирование)
        {
            const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
            if (state->selected_index < static_cast<long long>(display_files.size())) {
                fs_copy(state, display_files[state->selected_index].name);
            }
        }
            break;
        case 24: // Ctrl+X (вырезание)
        {
            const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
            if (state->selected_index < static_cast<long long>(display_files.size())) {
                fs_cut(state, display_files[state->selected_index].name);
            }
        }
            break;
        case 22: // Ctrl+V (вставка)
        {
            if (state->clipboard_path) {
                fs_paste(state, state->current_dir);
            }
        }
            break;
        case KEY_F(8): // Удаление
        {
            const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
            if (state->selected_index < static_cast<long long>(display_files.size())) {
                const FileInfo &file = display_files[state->selected_index];
                if (file.is_dir) {
                    fs_delete_dir(state, file.name);
                } else {
                    fs_delete_file(state, file.name);
                }
            }
        }
            break;
        case KEY_MOUSE:
            if (getmouse(&event) == OK) {
                unsigned int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                long long display_file_count = state->filtered_files.size() > 0 ? state->filtered_files.size() : state->files.size();
                const unsigned int start_y = 3; 
                if (event.y >= start_y && static_cast<long long>(event.y) < start_y + display_file_count && event.y < max_y - 2) {
                    state_select_index(state, event.y - start_y);
                    if (event.bstate & BUTTON1_CLICKED) {
                        const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
                        if (display_files[state->selected_index].is_dir) {
                            fs_open_dir(state, display_files[state->selected_index].name);
                        } else {
                            printf("Mouse click: attempting to open file: %s\n", display_files[state->selected_index].name);
                            fs_open_file(state, display_files[state->selected_index].name);
                        }
                    }
                }
            }
            break;
        case KEY_UP:
            if (state->selected_index > 0) {
                state->selected_index--;
                unsigned int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                size_t visible_rows = max_y - 3 - 2; // start_y = 3, 2 строки подсказки
                if (state->selected_index < state->scroll_y) {
                    state->scroll_y--;
                }
            }
            break;
        case KEY_DOWN:
        {
            long long max_index = state->filtered_files.size() > 0 ? state->filtered_files.size() - 1 : state->files.size() - 1;
            if (state->selected_index < max_index) {
                state->selected_index++;
                unsigned int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                size_t visible_rows = max_y - 3 - 2; // start_y = 3, 2 строки подсказки
                if (state->selected_index >= state->scroll_y + visible_rows) {
                    state->scroll_y++;
                }
            }
        }
            break;
        case 27: // Esc
            state_filter_files(state, NULL);
            break;
        case 's': // Сортировка по имени (по возрастанию)
            state->sort_type = SORT_BY_NAME_ASC;
            state_sort_files(state);
            break;
        case 'S': // Сортировка по имени (по убыванию)
            state->sort_type = SORT_BY_NAME_DESC;
            state_sort_files(state);
            break;
        case 'z': // Сортировка по размеру (по возрастанию)
            state->sort_type = SORT_BY_SIZE_ASC;
            state_sort_files(state);
            break;
        case 'Z': // Сортировка по размеру (по убыванию)
            state->sort_type = SORT_BY_SIZE_DESC;
            state_sort_files(state);
            break;
        case 'd': // Сортировка по дате (по возрастанию)
            state->sort_type = SORT_BY_DATE_ASC;
            state_sort_files(state);
            break;
        case 'D': // Сортировка по дате (по убыванию)
            state->sort_type = SORT_BY_DATE_DESC;
            state_sort_files(state);
            break;
    }
    if(quit)
        return 0;
    return 1;
}

int search_handle_input(AppState *state) {
   
    int ch = getch();
    if (ch == 'q') { // Выход по q
        return 0;
    }else if (ch == KEY_UP) {
        if (state->selected_index > 0) {
            state->selected_index--;
            unsigned int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            size_t visible_rows = max_y - 3 - 2; // start_y = 3, строка ввода + подсказка
            if (state->selected_index < state->scroll_y) {
                state->scroll_y--;
            }
        }
        ui_draw(state);
    } else if (ch == KEY_DOWN) {
        long long max_index = state->filtered_files.size() - 1;
        if (state->selected_index < max_index) {
            state->selected_index++;
            unsigned int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            size_t visible_rows = max_y - 3 - 2; // start_y = 3, строка ввода + подсказка
            if (state->selected_index >= state->scroll_y + visible_rows) {
                state->scroll_y++;
            }
        }
        ui_draw(state);
    } else if (ch == 27) { // Esc
        state->search_input[0] = '\0';
        state->search_active = false;
        state_filter_files(state, NULL);
        state->mode = MODE_BROWSE;
    } else if (ch == '\n') {
        state_filter_files(state, state->search_input);
        state->search_input[0] = '\0';
        state->search_active = false;
        state->mode = MODE_BROWSE;
    } else if (ch == KEY_BACKSPACE && strlen(state->search_input) > 0) {
        state->search_input[strlen(state->search_input) - 1] = '\0';
        state_filter_files(state, state->search_input);
        ui_draw(state);
    } else if (ch >= 32 && ch <= 126 && strlen(state->search_input) < 256 - 1) {
        size_t len = strlen(state->search_input);
        state->search_input[len] = static_cast<char>(ch);
        state->search_input[len + 1] = '\0';
        state_filter_files(state, state->search_input);
        ui_draw(state);
    }
    return 1;
}

int analysis_handle_input(AppState *state) {
    if (!state || !state->analysis_result) {
        printf("Error: analysis_handle_input called with NULL state or result\n");
        return 0;
    }

    int ch = getch();
    AnalysisResult *result = state->analysis_result;

    size_t total_items = result->old_files.size() + result->empty_files.size() + result->empty_dirs.size();
    for (const auto &dup : result->duplicates) {
        if (dup.paths.size() > 1) total_items += dup.paths.size();
    }

    switch (ch) {
        case 'q':
            return 0;
        case 27: // Esc
            state->mode = MODE_BROWSE;
            state_filter_files(state, NULL); // Добавляем сброс поиска
            break;
        case KEY_UP:
            if (result->selected_index > 0) {
                result->selected_index--;
                unsigned int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                size_t visible_rows = max_y - 2 - 1; // y=2 после заголовка, 1 строка подсказки
                if (result->selected_index < state->scroll_y) {
                    state->scroll_y--;
                }
                // Обновляем секцию
                size_t idx = result->selected_index;
                if (idx < result->old_files.size()) {
                    result->section = AnalysisResult::SECTION_OLD;
                } else if (idx < result->old_files.size() + result->empty_files.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_FILES;
                } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_DIRS;
                } else {
                    result->section = AnalysisResult::SECTION_DUPLICATES;
                }
            }
            break;
        case KEY_DOWN:
            if (result->selected_index + 1 < total_items) {
                result->selected_index++;
                unsigned int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                size_t visible_rows = max_y - 2 - 1; // y=2 после заголовка, 1 строка подсказки
                if (result->selected_index >= state->scroll_y + visible_rows) {
                    state->scroll_y++;
                }
                // Обновляем секцию
                size_t idx = result->selected_index;
                if (idx < result->old_files.size()) {
                    result->section = AnalysisResult::SECTION_OLD;
                } else if (idx < result->old_files.size() + result->empty_files.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_FILES;
                } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_DIRS;
                } else {
                    result->section = AnalysisResult::SECTION_DUPLICATES;
                }
            }
            break;
        case '\n': // Enter
            {
                std::string selected_path;
                size_t idx = result->selected_index;
                if (idx < result->old_files.size()) {
                    selected_path = result->old_files[idx];
                } else if (idx < result->old_files.size() + result->empty_files.size()) {
                    selected_path = result->empty_files[idx - result->old_files.size()];
                } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    // Пустые директории не открываем в редакторе
                    break;
                } else {
                    size_t dup_idx = idx - (result->old_files.size() + result->empty_files.size() + result->empty_dirs.size());
                    size_t current = 0;
                    for (const auto &dup : result->duplicates) {
                        if (dup.paths.size() > 1) {
                            for (size_t i = 0; i < dup.paths.size(); i++) {
                                if (current == dup_idx) {
                                    selected_path = dup.paths[i];
                                    break;
                                }
                                current++;
                            }
                        }
                        if (!selected_path.empty()) break;
                    }
                }
                if (!selected_path.empty()) {
                    state_set_edit_file(state, selected_path.c_str());
                    state->mode = MODE_EDITOR;
                }
            }
            break;
        case KEY_F(4): // F4 для удаления
            {
                std::string selected_path;
                size_t idx = result->selected_index;
                bool is_dir = false;
                if (idx < result->old_files.size()) {
                    selected_path = result->old_files[idx];
                } else if (idx < result->old_files.size() + result->empty_files.size()) {
                    selected_path = result->empty_files[idx - result->old_files.size()];
                } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    selected_path = result->empty_dirs[idx - (result->old_files.size() + result->empty_files.size())];
                    is_dir = true;
                } else {
                    size_t dup_idx = idx - (result->old_files.size() + result->empty_files.size() + result->empty_dirs.size());
                    size_t current = 0;
                    for (const auto &dup : result->duplicates) {
                        if (dup.paths.size() > 1) {
                            for (size_t i = 0; i < dup.paths.size(); i++) {
                                if (current == dup_idx) {
                                    selected_path = dup.paths[i];
                                    break;
                                }
                                current++;
                            }
                        }
                        if (!selected_path.empty()) break;
                    }
                }
                if (!selected_path.empty()) {
                    mvprintw(0, 0, "Delete %s? (y/n)", selected_path.c_str());
                    refresh();
                    int confirm = getch();
                    if (confirm == 'y' || confirm == 'Y') {
                        int ret = is_dir ? rmdir(selected_path.c_str()) : unlink(selected_path.c_str());
                        if (ret == 0) {
                            printf("Deleted %s: %s\n", is_dir ? "directory" : "file", selected_path.c_str());
                            // Удаляем из результатов
                            if (idx < result->old_files.size()) {
                                result->old_files.erase(result->old_files.begin() + idx);
                            } else if (idx < result->old_files.size() + result->empty_files.size()) {
                                result->empty_files.erase(result->empty_files.begin() + (idx - result->old_files.size()));
                            } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                                result->empty_dirs.erase(result->empty_dirs.begin() + (idx - (result->old_files.size() + result->empty_files.size())));
                            } else {
                                size_t dup_idx = idx - (result->old_files.size() + result->empty_files.size() + result->empty_dirs.size());
                                size_t current = 0;
                                for (auto &dup : result->duplicates) {
                                    if (dup.paths.size() > 1) {
                                        for (size_t i = 0; i < dup.paths.size(); i++) {
                                            if (current == dup_idx) {
                                                dup.paths.erase(dup.paths.begin() + i);
                                                break;
                                            }
                                            current++;
                                        }
                                    }
                                }
                            }
                            if (result->selected_index > 0 && result->selected_index >= total_items - 1) {
                                result->selected_index--;
                            }
                        } else {
                            printf("Failed to delete %s: %s\n", is_dir ? "directory" : "file", selected_path.c_str());
                        }
                    }
                }
            }
            break;
    }
    return 1;
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
       case 'q': // выход
            if (editor->is_modified) {
                clear();
                mvprintw(0, 0, "You have unsaved changes. Exit without saving? (y/n)");
                refresh();
                int confirm = getch();
                if (confirm != 'y' && confirm != 'Y') {
                    break;
                }
            }
            editor_free(editor);
            free(editor);
            state->editor_state = NULL;
            state_set_edit_file(state, NULL);
            return 0;
       case 27: // Esc
            if (editor->is_modified) {
                clear();
                mvprintw(0, 0, "You have unsaved changes. Exit without saving? (y/n)");
                refresh();
                int confirm = getch();
                if (confirm != 'y' && confirm != 'Y') {
                    break; 
                }
            }
            editor_free(editor);
            free(editor);
            state->editor_state = NULL;
            state->mode = MODE_BROWSE;
            state_set_edit_file(state, NULL);
            state_filter_files(state, NULL); 
            break;
        case KEY_F(2): // Сохранение
            if (state->edit_file) {
                FILE *file = fopen(state->edit_file, "w");
                if (!file) {
                    fprintf(stderr, "Failed to save file: %s\n", state->edit_file);
                    break;
                }
                for (size_t i = 0; i < editor->line_count; ++i) {
                    if (editor->lines[i]) {
                        fprintf(file, "%s\n", editor->lines[i]);
                    } else {
                        fprintf(file, "\n");
                    }
                }
                fclose(file);
                editor->is_modified = false;
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
                editor->is_modified = true; // Добавляем
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
                editor->is_modified = true; // Добавляем
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
            editor->is_modified = true; // Добавляем
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
                editor->is_modified = true; 
            }
            break;
    }

    return 1;
}