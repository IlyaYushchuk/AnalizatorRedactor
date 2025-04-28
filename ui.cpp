#include "ui.h"
#include "filesystem.h"
#include "editor.h"
#include "analysis.h"
#include <locale.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

void ui_init() {
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_WHITE, COLOR_BLACK);
}

void ui_deinit() {
    endwin();
}

void ui_draw(AppState *state) {
    if (!state) {
        fprintf(stderr, "Error: ui_draw called with NULL state\n");
        return;
    }
    clear();

    if (state->mode == MODE_BROWSE) {
        mvprintw(0, 0, "Directory: %s", state->current_dir ? state->current_dir : "(null)");

        // Отображаем статус поиска
        if (state->search_query && strlen(state->search_query) > 0) {
            attron(COLOR_PAIR(2));
            mvprintw(1, 0, "Режим поиска: '%s' (рекурсивный)", state->search_query);
            attroff(COLOR_PAIR(2));
        }

        unsigned int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        unsigned int start_y = state->search_query ? 3 : 2; // Смещаем список если есть поиск

        // Используем отфильтрованные файлы, если есть поиск
        const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;

        for (size_t i = 0; i < display_files.size() && start_y + i < max_y - 2; i++) {
            const FileInfo &file = display_files[i];
            bool is_selected = (static_cast<long long>(i) == state->selected_index);

            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&file.mtime));

            if (is_selected) {
                attron(COLOR_PAIR(1));
            } else if (file.is_dir) {
                attron(COLOR_PAIR(2));
            } else {
                attron(COLOR_PAIR(3));
            }

            // Ограничиваем длину имени файла
            char display_name[100];
            snprintf(display_name, sizeof(display_name), "%.90s", file.name ? file.name : "(null)");
            mvprintw(start_y + i, 0, "%-50s %10lld %s", display_name, file.size, time_str);

            if (is_selected) {
                attroff(COLOR_PAIR(1));
            } else if (file.is_dir) {
                attroff(COLOR_PAIR(2));
            } else {
                attroff(COLOR_PAIR(3));
            }
        }

        // Статусная строка
        mvprintw(max_y - 1, 0, "q: Quit | Enter: Open | Arrows: Navigate | F3: Analyze | f: Search");
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
        int ch = getch();
        MEVENT event;

        // Режим ввода поискового запроса
        static char search_input[256] = "";
        static bool search_active = false;

        if (search_active) {
            if (ch == 27) { // Esc
                search_input[0] = '\0';
                search_active = false;
                state_filter_files(state, NULL);
            } else if (ch == '\n') {
                state_filter_files(state, search_input);
                search_input[0] = '\0';
                search_active = false;
            } else if (ch == KEY_BACKSPACE && strlen(search_input) > 0) {
                search_input[strlen(search_input) - 1] = '\0';
                state_filter_files(state, search_input);
            } else if (ch >= 32 && ch <= 126 && strlen(search_input) < sizeof(search_input) - 1) {
                search_input[strlen(search_input)] = static_cast<char>(ch);
                search_input[strlen(search_input)] = '\0';
                state_filter_files(state, search_input);
            }
            return 1;
        }

        switch (ch) {
            case 'q':
                return 0;
            case KEY_UP:
                state_select_index(state, state->selected_index > 0 ? state->selected_index - 1 : 0);
                break;
            case KEY_DOWN:
            {
                long long max_index = state->filtered_files.size() > 0 ? state->filtered_files.size() - 1 : state->files.size() - 1;
                state_select_index(state, state->selected_index + 1 <= max_index ? state->selected_index + 1 : max_index);
            }
                break;
            case '\n':
            {
                const std::vector<FileInfo> &display_files = state->filtered_files.size() > 0 ? state->filtered_files : state->files;
                if (state->selected_index < static_cast<long long>(display_files.size())) {
                    if (display_files[state->selected_index].is_dir) {
                        fs_open_dir(state, display_files[state->selected_index].name);
                    } else {
                        printf("Attempting to open file: %s\n", display_files[state->selected_index].name);
                        fs_open_file(state, display_files[state->selected_index].name);
                    }
                }
            }
                break;
            case 'f': // Активация поиска
                search_input[0] = '\0';
                search_active = true;
                state_filter_files(state, search_input);
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
            case KEY_MOUSE:
                if (getmouse(&event) == OK) {
                    unsigned int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);
                    long long display_file_count = state->filtered_files.size() > 0 ? state->filtered_files.size() : state->files.size();
                    if (event.y >= 2 && static_cast<long long>(event.y) < 2 + display_file_count && event.y < max_y - 1) {
                        state_select_index(state, event.y - 2);
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
        }
    } else if (state->mode == MODE_EDITOR) {
        return editor_handle_input(state);
    } else if (state->mode == MODE_ANALYSIS) {
        return analysis_handle_input(state);
    }
    return 1;
}