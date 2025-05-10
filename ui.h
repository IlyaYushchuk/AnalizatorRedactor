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
std::wstring ui_show_file_create_dialog(AppState *state);
std::wstring ui_show_dir_create_dialog(AppState *state);
std::wstring ui_show_rename_dialog(AppState *state, const char *old_name);
std::wstring ui_show_analysis_days_dialog(AppState *state);
bool ui_show_confirm_delete_dialog(AppState *state, const char *name, bool is_dir);
void ui_show_error_dialog(AppState *state, const char *message);

#endif