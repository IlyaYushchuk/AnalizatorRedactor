#include "ui.h"
#include "filesystem.h"
#include "editor.h"
#include "analysis.h"
#include "handlers.h"
#include <sys/stat.h>
#include <locale.h>
#include <string.h>
#include <time.h>
#include <string>

// Подсчитывает видимую ширину строки UTF-8
static int get_display_width(const char *str) {
    if (!str) return 0;
    int width = 0;
    wchar_t wch;
    mbstate_t mbs;
    memset(&mbs, 0, sizeof(mbs));

    size_t len = strlen(str);
    size_t i = 0;
    while (i < len) {
        size_t ret = mbrtowc(&wch, str + i, len - i, &mbs);
        if (ret == (size_t)-1 || ret == (size_t)-2) break; // Ошибка
        if (ret == 0) break; // Конец строки
        width += wcwidth(wch);
        i += ret;
    }
    return width;
}

// Преобразует строку UTF-8 в массив широких символов (wchar_t)
static void utf8_to_wchar(const char *str, wchar_t *wstr, size_t max_len) {
    mbstate_t mbs;
    memset(&mbs, 0, sizeof(mbs));
    mbsrtowcs(wstr, &str, max_len, &mbs);
    wstr[max_len - 1] = L'\0'; // Гарантируем завершение строки
}

// Обрезает строку до заданной видимой ширины и преобразует в широкие символы
static void truncate_to_width(const char *str, wchar_t *wstr, int target_width, size_t max_len) {
    if (!str) {
        wstr[0] = L'\0';
        return;
    }

    wchar_t temp[max_len];
    utf8_to_wchar(str, temp, max_len);

    int width = 0;
    size_t pos = 0;
    for (size_t i = 0; temp[i] != L'\0' && i < max_len - 1; i++) {
        int char_width = wcwidth(temp[i]);
        if (width + char_width > target_width) break;
        width += char_width;
        wstr[pos++] = temp[i];
    }
    wstr[pos] = L'\0';

    // Дополняем пробелами до target_width
    while (width < target_width && pos < max_len - 1) {
        wstr[pos++] = L' ';
        width++;
    }
    wstr[pos] = L'\0';
}

void ui_init() {
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    init_pair(1, COLOR_GREEN, COLOR_BLACK);  // Выделенный элемент
    init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Директории
    init_pair(3, COLOR_WHITE, COLOR_BLACK);   // Обычные файлы
    init_pair(4, COLOR_CYAN, COLOR_BLACK);   // Исполняемые файлы
    init_pair(5, COLOR_RED, COLOR_BLACK);    // Архивы (.zip, .tar, etc.)
}

void ui_deinit() {
    endwin();
}

static void browse_draw(AppState *state) {
    mvprintw(0, 0, "Directory: %s", state->current_dir ? state->current_dir : "(null)");

    unsigned int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    unsigned int start_y = 2;

    // Определяем ширину столбцов
    const int name_width = 50; // Ширина для имени файла/папки
    const int size_width = 10; // Ширина для размера
    const int date_width = 20; // Ширина для даты

    // Рисуем заголовки столбцов
    wchar_t name_header[100], size_header[100], date_header[100];
    truncate_to_width("Имя файла/папки", name_header, name_width, 100);
    truncate_to_width("Размер", size_header, size_width, 100);
    truncate_to_width("Дата", date_header, date_width, 100);

    attron(COLOR_PAIR(3));
    move(start_y, 0);
    addwstr(name_header);
    addwstr(size_header);
    addwstr(date_header);
    attroff(COLOR_PAIR(3));
    start_y++;

    const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;

    for (size_t i = 0; i < display_files.size() && start_y + i < max_y - 3; i++) {
        const FileInfo &file = display_files[i];
        bool is_selected = (static_cast<long long>(i) == state->selected_index);

        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&file.mtime));

        char size_str[20];
        snprintf(size_str, sizeof(size_str), "%lld", file.size);

        wchar_t display_name_w[100];
        wchar_t display_size[20];
        wchar_t display_date[30];

        const char *display_name = file.name ? file.name : "(null)";
        if (state->filtered_files.size() > 0) {
            const char *last_slash = strrchr(display_name, '/');
            if (last_slash) display_name = last_slash + 1;
        }
        truncate_to_width(display_name, display_name_w, name_width, 100);
        truncate_to_width(size_str, display_size, size_width, 20);
        truncate_to_width(time_str, display_date, date_width, 30);

        // Определяем тип файла для подсветки
        int color_pair = 3; // Обычный файл по умолчанию
        if (is_selected) {
            color_pair = 1; // Выделенный элемент
        } else if (file.is_dir) {
            color_pair = 2; // Директория
        } else {
            const char *ext = strrchr(file.name, '.');
            if (ext) {
                if (strcmp(ext, ".zip") == 0 || strcmp(ext, ".tar") == 0 || strcmp(ext, ".gz") == 0) {
                    color_pair = 5; // Архивы
                }
            }
            struct stat st;
            if (stat(file.name, &st) != -1 && (st.st_mode & S_IXUSR)) {
                color_pair = 4; // Исполняемый файл
            }
        }

        attron(COLOR_PAIR(color_pair));
        move(start_y + i, 0);
        addwstr(display_name_w);
        addwstr(display_size);
        addwstr(display_date);
        attroff(COLOR_PAIR(color_pair));
    }

    mvprintw(max_y - 2, 0, "q: Quit | Enter: Open | Arrows: Navigate | F3: Analyze | f: Search | F5: Create File | F6: Create Dir");
    mvprintw(max_y - 1, 0, "F7: Rename | Ctrl+C: Copy | Ctrl+X: Cut | Ctrl+V: Paste | F8: Delete | s/S: Sort by Name | z/Z: Sort by Size | d/D: Sort by Date");
}

static void search_draw(AppState *state) {
    mvprintw(0, 0, "Directory: %s", state->current_dir ? state->current_dir : "(null)");

    attron(COLOR_PAIR(2));
    mvprintw(1, 0, "Search mode: '%s' (recursive)", state->search_input);
    attroff(COLOR_PAIR(2));

    unsigned int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    unsigned int start_y = 3;

    // Определяем ширину столбцов
    const int name_width = 50; // Ширина для имени файла/папки
    const int size_width = 10; // Ширина для размера
    const int date_width = 20; // Ширина для даты

    // Рисуем заголовки столбцов
    wchar_t name_header[100], size_header[100], date_header[100];
    truncate_to_width("Имя файла/папки", name_header, name_width, 100);
    truncate_to_width("Размер", size_header, size_width, 100);
    truncate_to_width("Дата", date_header, date_width, 100);

    attron(COLOR_PAIR(3));
    move(start_y, 0);
    addwstr(name_header);
    addwstr(size_header);
    addwstr(date_header);
    attroff(COLOR_PAIR(3));
    start_y++;

    const std::vector<FileInfo> &display_files = state->filtered_files;

    for (size_t i = 0; i < display_files.size() && start_y + i < max_y - 2; i++) {
        const FileInfo &file = display_files[i];
        bool is_selected = (static_cast<long long>(i) == state->selected_index);

        // Форматируем дату
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&file.mtime));

        // Форматируем размер
        char size_str[20];
        snprintf(size_str, sizeof(size_str), "%lld", file.size);

        // Преобразуем строки в широкие символы с учетом ширины столбцов
        wchar_t display_name[100];
        wchar_t display_size[20];
        wchar_t display_date[30];

        truncate_to_width(file.name ? file.name : "(null)", display_name, name_width, 100);
        truncate_to_width(size_str, display_size, size_width, 20);
        truncate_to_width(time_str, display_date, date_width, 30);

        // Выбор цвета
        if (is_selected) {
            attron(COLOR_PAIR(1));
        } else if (file.is_dir) {
            attron(COLOR_PAIR(2));
        } else {
            attron(COLOR_PAIR(3));
        }

        // Вывод строки
        move(start_y + i, 0);
        addwstr(display_name);
        addwstr(display_size);
        addwstr(display_date);

        // Сброс цвета
        if (is_selected) {
            attroff(COLOR_PAIR(1));
        } else if (file.is_dir) {
            attroff(COLOR_PAIR(2));
        } else {
            attroff(COLOR_PAIR(3));
        }
    }

    mvprintw(max_y - 1, 0, "q: Quit | Esc: Back | Enter: End Search | Backspace: Edit Query");
}

static void analysis_draw(AppState *state) {
    if (!state || !state->analysis_result) {
        printf("Error: analysis_draw called with NULL state or result\n");
        clear();
        mvprintw(0, 0, "Analysis failed: No results");
        refresh();
        return;
    }

    clear();
    unsigned int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int y = 0;

    mvprintw(y++, 0, "Analysis of directory: %s", state->current_dir);

    size_t total_items = state->analysis_result->old_files.size() +
                        state->analysis_result->empty_files.size() +
                        state->analysis_result->empty_dirs.size();
    for (const auto &dup : state->analysis_result->duplicates) {
        if (dup.paths.size() > 1) total_items += dup.paths.size();
    }

    y++;
    mvprintw(y++, 0, "Old files:");
    size_t item_index = 0;
    for (size_t i = 0; i < state->analysis_result->old_files.size(); i++) {
        if (y >= max_y - 2) break;
        bool selected = (state->analysis_result->section == AnalysisResult::SECTION_OLD &&
                         state->analysis_result->selected_index == item_index);
        if (selected) attron(COLOR_PAIR(1));
        mvprintw(y++, 0, "  %s", state->analysis_result->old_files[i].c_str());
        if (selected) attroff(COLOR_PAIR(1));
        item_index++;
    }

    y++;
    mvprintw(y++, 0, "Empty files:");
    for (size_t i = 0; i < state->analysis_result->empty_files.size(); i++) {
        if (y >= max_y - 2) break;
        bool selected = (state->analysis_result->section == AnalysisResult::SECTION_EMPTY_FILES &&
                         state->analysis_result->selected_index == item_index);
        if (selected) attron(COLOR_PAIR(1));
        mvprintw(y++, 0, "  %s", state->analysis_result->empty_files[i].c_str());
        if (selected) attroff(COLOR_PAIR(1));
        item_index++;
    }

    y++;
    mvprintw(y++, 0, "Empty directories:");
    for (size_t i = 0; i < state->analysis_result->empty_dirs.size(); i++) {
        if (y >= max_y - 2) break;
        bool selected = (state->analysis_result->section == AnalysisResult::SECTION_EMPTY_DIRS &&
                         state->analysis_result->selected_index == item_index);
        if (selected) attron(COLOR_PAIR(1));
        mvprintw(y++, 0, "  %s", state->analysis_result->empty_dirs[i].c_str());
        if (selected) attroff(COLOR_PAIR(1));
        item_index++;
    }

    y++;
    mvprintw(y++, 0, "Duplicate files:");
    for (const DuplicateInfo &dup : state->analysis_result->duplicates) {
        if (dup.paths.size() > 1) {
            if (y >= max_y - 2) break;
            mvprintw(y++, 0, "  Hash: %s", dup.hash.c_str());
            for (size_t i = 0; i < dup.paths.size(); i++) {
                if (y >= max_y - 2) break;
                bool selected = (state->analysis_result->section == AnalysisResult::SECTION_DUPLICATES &&
                                 state->analysis_result->selected_index == item_index);
                if (selected) attron(COLOR_PAIR(1));
                mvprintw(y++, 0, "    %s", dup.paths[i].c_str());
                if (selected) attroff(COLOR_PAIR(1));
                item_index++;
            }
        }
    }

    mvprintw(max_y - 1, 0, "q: Quit | Esc: Back | Arrows: Navigate | Enter: Open | F4: Delete");
    refresh();
}

static void editor_draw(AppState *state) {
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
}

void ui_draw(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: ui_draw called with NULL state\n");
        return;
    }
    clear();


    if (state->mode == MODE_BROWSE) {
        browse_draw(state);
    } else if (state->mode == MODE_SEARCH) {
        search_draw(state);
    } else if (state->mode == MODE_EDITOR) {
        editor_draw(state);
    } else if (state->mode == MODE_ANALYSIS) {
        analysis_draw(state);
    }

    refresh();
}

int ui_handle_input(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: ui_handle_input called with NULL state\n");
        return 0;
    }

   

    if (state->mode == MODE_BROWSE) {
        return browse_handle_input(state);
    } else if (state->mode == MODE_SEARCH) {
        return search_handle_input(state);
    } else if (state->mode == MODE_EDITOR) {
        return editor_handle_input(state);
    } else if (state->mode == MODE_ANALYSIS) {
        return analysis_handle_input(state);
    }
    return 1;
}