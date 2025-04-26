#ifndef UI_H
#define UI_H

#include "state.h"
#include <ncursesw/ncurses.h>

void ui_init();
void ui_deinit();
void ui_draw(AppState *state);
int ui_handle_input(AppState *state);

#endif