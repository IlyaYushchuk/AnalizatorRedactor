#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "state.h"

// Функции для работы с файловой системой
void fs_get_files(const char *path, FileInfo **files, long long *count);
void fs_open_dir(AppState *state, const char *dir_name);
void fs_open_file(AppState *state, const char *file_name);

#endif