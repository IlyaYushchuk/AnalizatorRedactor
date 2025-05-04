#ifndef UI_H
#define UI_H

#include "state.h"
#include <ncursesw/ncurses.h>
#include <openssl/md5.h>

void ui_init();
void ui_deinit();
void ui_draw(AppState *state);
int ui_handle_input(AppState *state);
void ui_show_metadata(AppState *state, const Metadata &meta);

#endif