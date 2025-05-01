#ifndef HANDLERS_H
#define HANDLERS_H

#include "state.h"
#include <ncursesw/ncurses.h>

// Обработчики ввода для каждого режима
int browse_handle_input(AppState *state);
int search_handle_input(AppState *state, char *search_input, bool *search_active);
int analysis_handle_input(AppState *state);
int editor_handle_input(AppState *state);

#endif