#include "filesystem.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h> 
#include <cwctype>   
#include <wctype.h>

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

        // Преобразуем строки в широкие символы для корректного сравнения
        wchar_t wname[PATH_MAX];
        wchar_t wquery[PATH_MAX];
        mbstowcs(wname, entry->d_name, PATH_MAX);
        mbstowcs(wquery, query, PATH_MAX);

        // Приводим к нижнему регистру для регистронезависимого сравнения
        for (size_t i = 0; wname[i]; i++) wname[i] = towlower(wname[i]);
        for (size_t i = 0; wquery[i]; i++) wquery[i] = towlower(wquery[i]);

        if (wcsstr(wname, wquery)) {
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

// Создание нового файла
void fs_create_file(AppState *state, const char *filename) {
    if (!state || !filename) {
        fprintf(stderr, "Error: fs_create_file called with NULL state or filename\n");
        return;
    }
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, filename);
    
    int fd = open(full_path, O_CREAT | O_WRONLY, 0644);
    if (fd == -1) {
        fprintf(stderr, "Failed to create file: %s (%s)\n", full_path, strerror(errno));
        return;
    }
    close(fd);
    printf("Created file: %s\n", full_path);
    state_load_files(state); // Обновляем список файлов
}

// Создание новой папки
void fs_create_dir(AppState *state, const char *dirname) {
    if (!state || !dirname) {
        fprintf(stderr, "Error: fs_create_dir called with NULL state or dirname\n");
        return;
    }
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, dirname);
    
    if (mkdir(full_path, 0755) == -1) {
        fprintf(stderr, "Failed to create directory: %s (%s)\n", full_path, strerror(errno));
        return;
    }
    printf("Created directory: %s\n", full_path);
    state_load_files(state); // Обновляем список файлов
}

// Переименование файла или папки
void fs_rename(AppState *state, const char *old_name, const char *new_name) {
    if (!state || !old_name || !new_name) {
        fprintf(stderr, "Error: fs_rename called with NULL state, old_name or new_name\n");
        return;
    }
    char old_path[PATH_MAX];
    char new_path[PATH_MAX];
    snprintf(old_path, sizeof(old_path), "%s/%s", state->current_dir, old_name);
    snprintf(new_path, sizeof(new_path), "%s/%s", state->current_dir, new_name);
    
    if (rename(old_path, new_path) == -1) {
        fprintf(stderr, "Failed to rename %s to %s (%s)\n", old_path, new_path, strerror(errno));
        return;
    }
    printf("Renamed %s to %s\n", old_path, new_path);
    state_load_files(state); // Обновляем список файлов
}

// Копирование файла/папки в буфер
void fs_copy(AppState *state, const char *src_path) {
    if (!state || !src_path) {
        fprintf(stderr, "Error: fs_copy called with NULL state or src_path\n");
        return;
    }
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, src_path);
    if (state->clipboard_path) {
        free(state->clipboard_path);
    }
    state->clipboard_path = strdup(full_path);
    if (!state->clipboard_path) {
        fprintf(stderr, "Failed to allocate memory for clipboard path\n");
        return;
    }
    state->clipboard_is_cut = false;
    printf("Copied to clipboard: %s\n", full_path);
}

// Вырезание файла/папки в буфер
void fs_cut(AppState *state, const char *src_path) {
    if (!state || !src_path) {
        fprintf(stderr, "Error: fs_cut called with NULL state or src_path\n");
        return;
    }
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, src_path);
    if (state->clipboard_path) {
        free(state->clipboard_path);
    }
    state->clipboard_path = strdup(full_path);
    if (!state->clipboard_path) {
        fprintf(stderr, "Failed to allocate memory for clipboard path\n");
        return;
    }
    state->clipboard_is_cut = true;
    printf("Cut to clipboard: %s\n", full_path);
}

// Рекурсивное копирование директории
static int copy_directory(const char *src_path, const char *dest_path) {
    if (mkdir(dest_path, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "Failed to create directory: %s (%s)\n", dest_path, strerror(errno));
        return -1;
    }

    DIR *dir = opendir(src_path);
    if (!dir) {
        fprintf(stderr, "Failed to open source directory: %s\n", src_path);
        return -1;
    }

    struct dirent *entry;
    char src_full_path[PATH_MAX];
    char dest_full_path[PATH_MAX];

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(src_full_path, sizeof(src_full_path), "%s/%s", src_path, entry->d_name);
        snprintf(dest_full_path, sizeof(dest_full_path), "%s/%s", dest_path, entry->d_name);

        struct stat st;
        if (stat(src_full_path, &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            if (copy_directory(src_full_path, dest_full_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else {
            FILE *src = fopen(src_full_path, "rb");
            if (!src) {
                fprintf(stderr, "Failed to open source file: %s\n", src_full_path);
                continue;
            }
            FILE *dst = fopen(dest_full_path, "wb");
            if (!dst) {
                fclose(src);
                fprintf(stderr, "Failed to open destination file: %s\n", dest_full_path);
                continue;
            }

            char buffer[4096];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                fwrite(buffer, 1, bytes, dst);
            }
            fclose(src);
            fclose(dst);
        }
    }
    closedir(dir);
    return 0;
}
// Рекурсивное удаление директории
static int remove_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Failed to open directory for deletion: %s\n", path);
        return -1;
    }

    struct dirent *entry;
    char full_path[PATH_MAX];
    int ret = 0;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            if (remove_directory(full_path) == -1) {
                ret = -1;
            }
        } else {
            if (unlink(full_path) == -1) {
                fprintf(stderr, "Failed to delete file in directory: %s\n", full_path);
                ret = -1;
            }
        }
    }
    closedir(dir);

    if (rmdir(path) == -1) {
        fprintf(stderr, "Failed to delete directory: %s\n", path);
        ret = -1;
    }
    return ret;
}

// Вставка файла/папки из буфера
void fs_paste(AppState *state, const char *dest_dir) {
    if (!state || !dest_dir || !state->clipboard_path) {
        fprintf(stderr, "Error: fs_paste called with NULL state, dest_dir, or clipboard_path\n");
        return;
    }

    char dest_path[PATH_MAX];
    const char *filename = strrchr(state->clipboard_path, '/');
    if (!filename) {
        filename = state->clipboard_path;
    } else {
        filename++;
    }
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, filename);

    struct stat st;
    if (stat(state->clipboard_path, &st) == -1) {
        fprintf(stderr, "Failed to stat source path: %s\n", state->clipboard_path);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        // Копирование директории
        if (copy_directory(state->clipboard_path, dest_path) == -1) {
            fprintf(stderr, "Failed to copy directory: %s to %s\n", state->clipboard_path, dest_path);
            return;
        }
        printf("Pasted directory to: %s\n", dest_path);
    } else {
        // Копирование файла (как было раньше)
        FILE *src = fopen(state->clipboard_path, "rb");
        if (!src) {
            fprintf(stderr, "Failed to open source file: %s\n", state->clipboard_path);
            return;
        }
        FILE *dst = fopen(dest_path, "wb");
        if (!dst) {
            fclose(src);
            fprintf(stderr, "Failed to open destination file: %s\n", dest_path);
            return;
        }

        char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }
        fclose(src);
        fclose(dst);
        printf("Pasted file to: %s\n", dest_path);
    }

    if (state->clipboard_is_cut) {
        if (S_ISDIR(st.st_mode)) {
            if (remove_directory(state->clipboard_path) == -1) {
                fprintf(stderr, "Failed to delete source directory after cut: %s\n", state->clipboard_path);
            } else {
                printf("Deleted source directory after cut: %s\n", state->clipboard_path);
            }
        } else {
            if (unlink(state->clipboard_path) == -1) {
                fprintf(stderr, "Failed to delete source file after cut: %s\n", state->clipboard_path);
            } else {
                printf("Deleted source file after cut: %s\n", state->clipboard_path);
            }
        }
    }

    free(state->clipboard_path);
    state->clipboard_path = NULL;
    state->clipboard_is_cut = false;
    state_load_files(state);
}
// Удаление файла с подтверждением
void fs_delete_file(AppState *state, const char *filename) {
    if (!state || !filename) {
        fprintf(stderr, "Error: fs_delete_file called with NULL state or filename\n");
        return;
    }
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, filename);

    // Запрашиваем подтверждение
    clear();
    mvprintw(0, 0, "Delete file %s? (y/n)", filename);
    refresh();
    int confirm = getch();
    if (confirm != 'y' && confirm != 'Y') {
        printf("File deletion canceled: %s\n", full_path);
        return;
    }

    if (unlink(full_path) == -1) {
        fprintf(stderr, "Failed to delete file: %s (%s)\n", full_path, strerror(errno));
        return;
    }
    printf("Deleted file: %s\n", full_path);
    state_load_files(state); // Обновляем список файлов
}


// Удаление директории с подтверждением
void fs_delete_dir(AppState *state, const char *dirname) {
    if (!state || !dirname) {
        fprintf(stderr, "Error: fs_delete_dir called with NULL state or dirname\n");
        return;
    }
    if (strcmp(dirname, "..") == 0) {
        fprintf(stderr, "Cannot delete parent directory '..'");
        return;
    }

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", state->current_dir, dirname);

    // Запрашиваем подтверждение
    clear();
    mvprintw(0, 0, "Delete directory %s and all its contents? (y/n)", dirname);
    refresh();
    int confirm = getch();
    if (confirm != 'y' && confirm != 'Y') {
        printf("Directory deletion canceled: %s\n", full_path);
        return;
    }

    if (remove_directory(full_path) == -1) {
        fprintf(stderr, "Failed to delete directory: %s\n", full_path);
        return;
    }
    printf("Deleted directory: %s\n", full_path);
    state_load_files(state); // Обновляем список файлов
}