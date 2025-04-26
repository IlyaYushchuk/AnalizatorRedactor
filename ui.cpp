#include "ui.h"
#include "filesystem.h"
#include "editor.h"
#include <locale.h>
#include <string.h>
#include <time.h>

void ui_init() {
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_WHITE, COLOR_BLACK);
}

void ui_deinit() {
    endwin();
}

void ui_draw(AppState *state) {
    clear();

    if (state->mode == MODE_BROWSE) {
        mvprintw(0, 0, "Directory: %s", state->current_dir);

        unsigned int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        unsigned int start_y = 2;

        for (size_t i = 0; i < state->file_count && start_y + i < max_y - 2; i++) {
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

            mvprintw(start_y + i, 0, "%-30s %10ld %s", file->name, file->size, time_str);

            if (is_selected) {
                attroff(COLOR_PAIR(1));
            } else if (file->is_dir) {
                attroff(COLOR_PAIR(2));
            } else {
                attroff(COLOR_PAIR(3));
            }
        }

        mvprintw(max_y - 1, 0, "q: Quit | Enter: Open | Arrows: Navigate | F1: Help");
    } else if (state->mode == MODE_EDITOR) {
        editor_draw(state);
    }

    refresh();
}

int ui_handle_input(AppState *state) {
    if (state->mode == MODE_BROWSE) {
        int ch = getch();
        MEVENT event;

        switch (ch) {
            case 'q':
                return 0;
            case KEY_UP:
                state_select_index(state, state->selected_index - 1);
                break;
            case KEY_DOWN:
                state_select_index(state, state->selected_index + 1);
                break;
            case '\n':
                if (state->files[state->selected_index].is_dir) {
                    fs_open_dir(state, state->files[state->selected_index].name);
                } else {
                    fs_open_file(state, state->files[state->selected_index].name);
                }
                break;
            case KEY_MOUSE:
                if (getmouse(&event) == OK) {
                    unsigned int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);
                    if (event.y >= 2 && event.y < 2 + state->file_count && event.y < max_y - 1) {
                        state_select_index(state, event.y - 2);
                        if (event.bstate & BUTTON1_CLICKED) {
                            if (state->files[state->selected_index].is_dir) {
                                fs_open_dir(state, state->files[state->selected_index].name);
                            } else {
                                fs_open_file(state, state->files[state->selected_index].name);
                            }
                        }
                    }
                }
                break;
        }
    } else if (state->mode == MODE_EDITOR) {
        return editor_handle_input(state);
    }
    return 1;
}