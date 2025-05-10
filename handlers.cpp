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
#include <sys/stat.h> 

int browse_handle_input(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: browse_handle_input called with NULL state\n");
        return 0;
    }

    int ch = getch();
    MEVENT event;
    bool quit = false;
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    switch (ch) {
        case 17: // Ctrl+Q
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
    case KEY_BACKSPACE:
        {
            // Проверяем, что мы не в корневой директории
            if (strcmp(state->current_dir, "/") != 0) {
                // Формируем путь к родительской директории
                char parent_dir[PATH_MAX];
                strncpy(parent_dir, state->current_dir, PATH_MAX);
                parent_dir[PATH_MAX - 1] = '\0';

                // Находим последний слэш
                char *last_slash = strrchr(parent_dir, '/');
                if (last_slash && last_slash != parent_dir) {
                    *last_slash = '\0'; // Обрезаем до родительской директории
                } else {
                    strcpy(parent_dir, "/"); // Если это корень, остаёмся в нём
                }

                // Переходим в родительскую директорию
                state_set_current_dir(state, parent_dir);
                state_load_files(state); // Обновляем список файлов
                state->selected_index = 0; // Сбрасываем выбор
                state->scroll_y = 0; // Сбрасываем вертикальную прокрутку
                state->scroll_x = 0; // Сбрасываем горизонтальную прокрутку (если добавлена)
                ui_draw(state); // Обновляем экран
            }
        }
            break;
        case 6: // Ctrl+F Поиск
            state->mode = MODE_SEARCH;
            state_filter_files(state, "");
            break;
        case KEY_F(3):
        {
            std::wstring input = ui_show_analysis_days_dialog(state);
            if (!input.empty()) {
                std::string days_str;
                char buf[MB_CUR_MAX];
                for (wchar_t wch : input) {
                    int len = wctomb(buf, wch);
                    if (len > 0) days_str.append(buf, len);
                }

                char *endptr;
                long days = strtol(days_str.c_str(), &endptr, 10);
                if (*endptr != '\0' || days < 0) {
                    ui_show_error_dialog(state, "Error: Invalid number of days");
                    days = 180;
                }

                time_t now = time(NULL);
                time_t old_threshold = now - days * 24 * 3600;
                analysis_perform(state, old_threshold);
            }
        }
            break;
        case KEY_F(5): // Создание файла
        {
            std::wstring input = ui_show_file_create_dialog(state);
            if (!input.empty()) {
                std::string filename;
                char buf[MB_CUR_MAX];
                for (wchar_t wch : input) {
                    int len = wctomb(buf, wch);
                    if (len > 0) filename.append(buf, len);
                }

                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, filename.c_str());
                struct stat st;
                if (stat(full_path, &st) == 0) {
                    std::string err_msg = "Error: File '" + filename + "' already exists";
                    ui_show_error_dialog(state, err_msg.c_str());
                } else {
                    fs_create_file(state, filename.c_str());
                }
            }
        }
            break;
        case KEY_F(6): // Создание папки
        {
            std::wstring input = ui_show_dir_create_dialog(state);
            if (!input.empty()) {
                std::string dirname;
                char buf[MB_CUR_MAX];
                for (wchar_t wch : input) {
                    int len = wctomb(buf, wch);
                    if (len > 0) dirname.append(buf, len);
                }

                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, dirname.c_str());
                struct stat st;
                if (stat(full_path, &st) == 0) {
                    std::string err_msg = "Error: Directory '" + dirname + "' already exists";
                    ui_show_error_dialog(state, err_msg.c_str());
                } else {
                    fs_create_dir(state, dirname.c_str());
                }
            }
        }
            break;
        case KEY_F(7): // Переименование
        {
            const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
            if (state->selected_index < static_cast<long long>(display_files.size())) {
                const char *old_name = display_files[state->selected_index].name;
                std::wstring input = ui_show_rename_dialog(state, old_name);
                if (!input.empty()) {
                    std::string new_name;
                    char buf[MB_CUR_MAX];
                    for (wchar_t wch : input) {
                        int len = wctomb(buf, wch);
                        if (len > 0) new_name.append(buf, len);
                    }

                    char full_path[PATH_MAX];
                    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, new_name.c_str());
                    struct stat st;
                    if (stat(full_path, &st) == 0) {
                        std::string err_msg = "Error: Name '" + new_name + "' already exists";
                        ui_show_error_dialog(state, err_msg.c_str());
                    } else {
                        fs_rename(state, old_name, new_name.c_str());
                    }
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
                if (ui_show_confirm_delete_dialog(state, file.name, file.is_dir)) {
                    if (file.is_dir) {
                        fs_delete_dir(state, file.name);
                    } else {
                        fs_delete_file(state, file.name);
                    }
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
        case 9: // Ctrl+I
        {
            const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
            if (state->selected_index < static_cast<long long>(display_files.size())) {
                const FileInfo &file = display_files[state->selected_index];
                // Формируем полный путь
                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, file.name);
                // Получаем метаданные
                Metadata meta = analysis_get_metadata(full_path);
                // Отображаем метаданные
                ui_show_metadata(state, meta);
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
    if (ch == 17) { // Выход по Ctrl + Q
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
        size_t len = strlen(state->search_input);
        size_t pos = len;
        while (pos > 0 && (state->search_input[pos - 1] & 0xC0) == 0x80) {
            pos--;
        }
        if (pos > 0) {
            state->search_input[pos - 1] = '\0';
            state_filter_files(state, state->search_input);
            ui_draw(state);
        }
    } else if (ch >= 32 && ch <= 126 && strlen(state->search_input) < 256 - 1) {
        // Обрабатываем многобайтовый ввод
        char buf[4] = {0}; // Достаточно для одного UTF-8 символа (максимум 4 байта)
        buf[0] = static_cast<char>(ch);
        size_t len = strlen(state->search_input);
        if (len < 256 - 4) { // Учитываем, что символ может быть до 4 байт
            // Проверяем, является ли ch началом UTF-8 символа
            if ((ch & 0xC0) != 0x80) { // Не продолжительный байт
                size_t bytes = 1;
                if ((ch & 0xE0) == 0xC0) bytes = 2; // 2-байтовый символ
                else if ((ch & 0xF0) == 0xE0) bytes = 3; // 3-байтовый символ
                else if ((ch & 0xF8) == 0xF0) bytes = 4; // 4-байтовый символ

                if (bytes == 1) {
                    // Однобайтовый символ
                    state->search_input[len] = ch;
                    state->search_input[len + 1] = '\0';
                } else {
                    // Многобайтовый символ
                    state->search_input[len] = ch;
                    for (size_t i = 1; i < bytes; i++) {
                        ch = wgetch(stdscr);
                        if (ch == ERR || (ch & 0xC0) != 0x80) {
                            // Некорректный UTF-8, отменяем
                            state->search_input[len] = '\0';
                            break;
                        }
                        state->search_input[len + i] = ch;
                    }
                    state->search_input[len + bytes] = '\0';
                }
                state_filter_files(state, state->search_input);
                ui_draw(state);
            }
        }
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
        case 17:
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
       case 17: // выход
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