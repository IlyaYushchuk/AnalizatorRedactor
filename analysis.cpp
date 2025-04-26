#include "analysis.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

// Инициализация результата анализа
void analysis_init(AnalysisResult *result) {
    result->old_files.clear();
    result->empty_files.clear();
    result->duplicates.clear();
}

// Освобождение памяти
void analysis_free(AnalysisResult *result) {
    for (char *path : result->old_files) {
        free(path);
    }
    for (char *path : result->empty_files) {
        free(path);
    }
    for (DuplicateInfo &dup : result->duplicates) {
        free(dup.hash);
        for (char *path : dup.paths) {
            free(path);
        }
    }
    result->old_files.clear();
    result->empty_files.clear();
    result->duplicates.clear();
}

// Вычисление MD5-хеша файла
static char *compute_md5(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;

    MD5_CTX ctx;
    MD5_Init(&ctx);
    unsigned char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        MD5_Update(&ctx, buffer, bytes);
    }
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);
    fclose(file);

    char *hash = (char *)malloc(33);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(hash + i * 2, "%02x", digest[i]);
    }
    hash[32] = '\0';
    return hash;
}

// Рекурсивный анализ директории
static void analyze_directory(const char *path, AnalysisResult *result, time_t old_threshold) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    struct stat st;
    char full_path[1024];

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            analyze_directory(full_path, result, old_threshold);
        } else if (S_ISREG(st.st_mode)) {
            // Пустые файлы
            if (st.st_size == 0) {
                result->empty_files.push_back(strdup(full_path));
            }
            // Давно не используемые файлы
            if (st.st_mtime < old_threshold) {
                result->old_files.push_back(strdup(full_path));
            }
            // Дубликаты
            char *hash = compute_md5(full_path);
            if (hash) {
                bool found = false;
                for (DuplicateInfo &dup : result->duplicates) {
                    if (strcmp(dup.hash, hash) == 0) {
                        dup.paths.push_back(strdup(full_path));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    DuplicateInfo dup;
                    dup.hash = hash;
                    dup.paths.push_back(strdup(full_path));
                    result->duplicates.push_back(dup);
                } else {
                    free(hash);
                }
            }
        }
    }

    closedir(dir);
}

// Выполнение анализа
void analysis_perform(AppState *state, time_t old_threshold) {
    if (!state->analysis_result) {
        state->analysis_result = (AnalysisResult *)malloc(sizeof(AnalysisResult));
        analysis_init(state->analysis_result);
    } else {
        analysis_free(state->analysis_result);
    }

    analyze_directory(state->current_dir, state->analysis_result, old_threshold);
    state->mode = MODE_ANALYSIS;
}

// Отрисовка результатов анализа
void analysis_draw(AppState *state) {
    if (!state->analysis_result) return;

    clear();
    unsigned int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int y = 0;

    mvprintw(y++, 0, "Analysis of directory: %s", state->current_dir);

    y++;
    mvprintw(y++, 0, "Old files (not modified in 6 months):");
    for (char *path : state->analysis_result->old_files) {
        if (y >= max_y - 2) break;
        mvprintw(y++, 0, "  %s", path);
    }

    y++;
    mvprintw(y++, 0, "Empty files:");
    for (char *path : state->analysis_result->empty_files) {
        if (y >= max_y - 2) break;
        mvprintw(y++, 0, "  %s", path);
    }

    y++;
    mvprintw(y++, 0, "Duplicate files:");
    for (const DuplicateInfo &dup : state->analysis_result->duplicates) {
        if (dup.paths.size() > 1) {
            if (y >= max_y - 2) break;
            mvprintw(y++, 0, "  Hash: %s", dup.hash);
            for (char *path : dup.paths) {
                if (y >= max_y - 2) break;
                mvprintw(y++, 0, "    %s", path);
            }
        }
    }

    mvprintw(max_y - 1, 0, "q: Quit | Esc: Back to Browse");
    refresh();
}

// Обработка ввода в режиме анализа
int analysis_handle_input(AppState *state) {
    int ch = getch();
    switch (ch) {
        case 'q':
            return 0; // Выход
        case 27: // Esc
            state->mode = MODE_BROWSE;
            break;
    }
    return 1;
}