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

        unsigned int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        unsigned int start_y = 2;

        for (size_t i = 0; i < state->file_count && start_y + i < static_cast<size_t>(max_y - 2); i++) {
            FileInfo *file = &state->files[i];
            bool is_selected = (i == state->selected_index);

            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&file->mtime));

            if (is_selected) {
                attron(COLOR_PAIR(1));
            } else if (file->is_dir) {
                attron(COLOR_PAIR(2));
            } else {
                attron(COLOR_PAIR(3));
            }

            mvprintw(start_y + i, 0, "%-30s %10ld %s", file->name ? file->name : "(null)", file->size, time_str);

            if (is_selected) {
                attroff(COLOR_PAIR(1));
            } else if (file->is_dir) {
                attroff(COLOR_PAIR(2));
            } else {
                attroff(COLOR_PAIR(3));
            }
        }

        mvprintw(max_y - 1, 0, "q: Quit | Enter: Open | Arrows: Navigate | F3: Analyze");
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

        switch (ch) {
            case 'q':
                return 0;
            case KEY_UP:
                state_select_index(state, state->selected_index > 0 ? state->selected_index - 1 : 0);
                break;
            case KEY_DOWN:
                state_select_index(state, state->selected_index + 1);
                break;
            case '\n':
                if (state->file_count > 0 && state->selected_index < state->file_count) {
                    if (state->files[state->selected_index].is_dir) {
                        fs_open_dir(state, state->files[state->selected_index].name);
                    } else {
                        printf("Attempting to open file: %s\n", state->files[state->selected_index].name);
                        fs_open_file(state, state->files[state->selected_index].name);
                    }
                }
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
                    if (event.y >= 2 && event.y < 2 + state->file_count && event.y < static_cast<size_t>(max_y - 1)) {
                        state_select_index(state, event.y - 2);
                        if (event.bstate & BUTTON1_CLICKED) {
                            if (state->files[state->selected_index].is_dir) {
                                fs_open_dir(state, state->files[state->selected_index].name);
                            } else {
                                printf("Mouse click: attempting to open file: %s\n", state->files[state->selected_index].name);
                                fs_open_file(state, state->files[state->selected_index].name);
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