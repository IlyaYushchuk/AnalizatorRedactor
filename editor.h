#ifndef EDITOR_H
#define EDITOR_H

#include "state.h" 
#include <ncursesw/ncurses.h>



// Функции редактора
void editor_init(EditorState *editor);
void editor_free(EditorState *editor);
void editor_load_file(EditorState *editor, const char *filename);
void editor_draw(AppState *state);
int editor_handle_input(AppState *state);

#endif