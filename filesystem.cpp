#include "filesystem.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

void fs_get_files(const char *path, FileInfo **files, long long *file_count) {
    if (!path || !files || !file_count) {
        fprintf(stderr, "Error: fs_get_files called with NULL arguments\n");
        return;
    }
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", path);
        return;
    }

    struct dirent *entry;
    struct stat st;
    long long capacity = 10;
    *files = (FileInfo *)malloc(capacity * sizeof(FileInfo));
    if (!*files) {
        fprintf(stderr, "Failed to allocate memory for files\n");
        closedir(dir);
        return;
    }
    *file_count = 0;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (stat(full_path, &st) == -1) continue;

        if (*file_count >= capacity) {
            capacity *= 2;
            FileInfo *new_files = (FileInfo *)realloc(*files, capacity * sizeof(FileInfo));
            if (!new_files) {
                fprintf(stderr, "Failed to reallocate memory for files\n");
                for (long long i = 0; i < *file_count; i++) {
                    free((*files)[i].name);
                }
                free(*files);
                *files = NULL;
                *file_count = 0;
                closedir(dir);
                return;
            }
            *files = new_files;
        }

        FileInfo *file = &(*files)[*file_count];
        file->name = strdup(entry->d_name);
        if (!file->name) {
            fprintf(stderr, "Failed to duplicate file name\n");
            continue;
        }
        file->is_dir = S_ISDIR(st.st_mode);
        file->size = st.st_size;
        file->mtime = st.st_mtime;
        (*file_count)++;
    }

    closedir(dir);
}

void fs_open_dir(AppState *state, const char *dir_name) {
    if (!state || !dir_name) {
        fprintf(stderr, "Error: fs_open_dir called with NULL state or dir_name\n");
        return;
    }
    char new_path[1024];
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
}

void fs_open_file(AppState *state, const char *file_name) {
    if (!state || !file_name) {
        fprintf(stderr, "Error: fs_open_file called with NULL state or file_name\n");
        return;
    }
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, file_name);
    printf("Opening file: %s\n", full_path);
    state_set_edit_file(state, full_path);
    state->mode = MODE_EDITOR;
}