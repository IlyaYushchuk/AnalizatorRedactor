#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "state.h"
#include <vector>

// Функции для работы с файловой системой
void fs_get_files(const char *path, std::vector<FileInfo> &files);
void fs_open_dir(AppState *state, const char *dir_name);
void fs_open_file(AppState *state, const char *file_name);
void fs_search_recursive(const char *base_path, const char *query, std::vector<FileInfo> &results);

#endif