#include "filesystem.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

void fs_get_files(const char *path, FileInfo **files, size_t *count) {
    DIR *dir = opendir(path);
    if (!dir) return;

    // Подсчитываем количество файлов/папок
    size_t capacity = 10;
    *files = (FileInfo *)malloc(capacity * sizeof(FileInfo));
    *count = 0;

    // Добавляем ".." для возврата назад
    (*files)[0].name = strdup("..");
    (*files)[0].is_dir = 1;
    (*files)[0].size = 0;
    (*files)[0].mtime = 0;
    *count = 1;

    struct dirent *entry;
    struct stat st;
    char full_path[1024];

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &st) == -1) continue;

        if (*count >= capacity) {
            capacity *= 2;
            *files = (FileInfo *)realloc(*files, capacity * sizeof(FileInfo));
        }

        (*files)[*count].name = strdup(entry->d_name);
        (*files)[*count].is_dir = S_ISDIR(st.st_mode);
        (*files)[*count].size = st.st_size;
        (*files)[*count].mtime = st.st_mtime;
        (*count)++;
    }

    closedir(dir);
}

void fs_open_dir(AppState *state, const char *dir_name) {
    char new_path[1024];
    if (strcmp(dir_name, "..") == 0) {
        // Перейти на уровень выше
        char *last_slash = strrchr(state->current_dir, '/');
        if (last_slash && last_slash != state->current_dir) {
            *last_slash = '\0';
            snprintf(new_path, sizeof(new_path), "%s", state->current_dir);
        } else {
            snprintf(new_path, sizeof(new_path), "/");
        }
    } else {
        snprintf(new_path, sizeof(new_path), "%s/%s", state->current_dir, dir_name);
    }
    state_set_current_dir(state, new_path);
}

void fs_open_file(AppState *state, const char *file_name) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, file_name);
    printf("Opening file: %s\n", full_path); // Отладочный вывод
    state_set_edit_file(state, full_path);
    state->mode = MODE_EDITOR;
}