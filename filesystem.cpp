#include "filesystem.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <vector>

void add_parent_dir_entry(std::vector<FileInfo> &files, const char *path) {
    // Проверяем, не корневая ли директория
    if (strcmp(path, "/") != 0) {
        FileInfo parent;
        parent.name = strdup("..");
        if (!parent.name) {
            fprintf(stderr, "Failed to duplicate parent dir name\n");
            return;
        }
        parent.is_dir = 1;
        parent.size = 0;
        parent.mtime = 0;
        files.insert(files.begin(), parent); // Вставляем ".." в начало
    }
}

void fs_get_files(const char *path, std::vector<FileInfo> &files) {
    if (!path) {
        fprintf(stderr, "Error: fs_get_files called with NULL path\n");
        return;
    }
    files.clear(); // Очищаем вектор перед заполнением

    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", path);
        return;
    }

    // Добавляем ".." в начало списка
    add_parent_dir_entry(files, path);

    struct dirent *entry;
    struct stat st;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (stat(full_path, &st) == -1) continue;

        FileInfo file;
        file.name = strdup(entry->d_name);
        if (!file.name) {
            fprintf(stderr, "Failed to duplicate file name\n");
            continue;
        }
        file.is_dir = S_ISDIR(st.st_mode);
        file.size = st.st_size;
        file.mtime = st.st_mtime;
        files.push_back(file);
    }

    closedir(dir);
}

void fs_search_recursive(const char *base_path, const char *query, std::vector<FileInfo> &results) {
    if (!base_path || !query) {
        fprintf(stderr, "Error: NULL arguments in fs_search_recursive\n");
        return;
    }

    DIR *dir = opendir(base_path);
    if (!dir) return;

    struct dirent *entry;
    struct stat st;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        if (stat(full_path, &st) == -1)
            continue;

        // Если это директория - рекурсивно ищем в ней
        if (S_ISDIR(st.st_mode)) {
            fs_search_recursive(full_path, query, results);
            continue;
        }

        // Проверяем совпадение с поисковым запросом
        if (strcasestr(entry->d_name, query)) {
            FileInfo file;
            file.name = strdup(full_path); // Сохраняем полный путь
            if (!file.name) {
                fprintf(stderr, "Failed to allocate memory for filename\n");
                continue;
            }
            file.is_dir = 0;
            file.size = st.st_size;
            file.mtime = st.st_mtime;
            results.push_back(file);
        }
    }

    closedir(dir);
}

void fs_open_dir(AppState *state, const char *dir_name) {
    if (!state || !dir_name) {
        fprintf(stderr, "Error: fs_open_dir called with NULL state or dir_name\n");
        return;
    }
    char new_path[PATH_MAX];
    if (strcmp(dir_name, "..") == 0) {
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
    state_filter_files(state, NULL); // Сбрасываем поиск при смене директории
}

void fs_open_file(AppState *state, const char *file_name) {
    if (!state || !file_name) {
        fprintf(stderr, "Error: fs_open_file called with NULL state or file_name\n");
        return;
    }
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, file_name);
    printf("Opening file: %s\n", full_path);
    state_set_edit_file(state, full_path);
    state->mode = MODE_EDITOR;
}