#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "state.h"
#include <vector>

// Функции для работы с файловой системой
void fs_get_files(const char *path, std::vector<FileInfo> &files);
void fs_open_dir(AppState *state, const char *dir_name);
void fs_open_file(AppState *state, const char *file_name);
void fs_search_recursive(const char *base_path, const char *query, std::vector<FileInfo> &results);
void fs_create_file(AppState *state, const char *filename);
void fs_create_dir(AppState *state, const char *dirname);
void fs_rename(AppState *state, const char *old_name, const char *new_name);
void fs_copy(AppState *state, const char *src_path);
void fs_paste(AppState *state, const char *dest_dir);
void fs_cut(AppState *state, const char *src_path);

#endif