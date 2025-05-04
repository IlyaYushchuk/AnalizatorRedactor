#include "analysis.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <stack>
#include <unistd.h>
#include <ctime>
#include <pwd.h>
#include <grp.h>

void analysis_init(AnalysisResult *result) {
    if (!result) {
        printf("Error: analysis_init called with NULL result\n");
        return;
    }
    result->old_files.clear();
    result->empty_files.clear();
    result->empty_dirs.clear();
    result->duplicates.clear();
    result->selected_index = 0;
    result->section = AnalysisResult::SECTION_OLD;
}

void analysis_free(AnalysisResult *result) {
    if (!result) {
        printf("Error: analysis_free called with NULL result\n");
        return;
    }
    result->old_files.clear();
    result->empty_files.clear();
    result->empty_dirs.clear();
    result->duplicates.clear();
}

static std::string compute_md5(const std::string &path) {
    printf("Computing MD5 for: %s\n", path.c_str());
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        printf("Failed to open file: %s\n", path.c_str());
        return "";
    }

    MD5_CTX ctx;
    if (!MD5_Init(&ctx)) {
        printf("Failed to initialize MD5 context\n");
        fclose(file);
        return "";
    }
    unsigned char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        MD5_Update(&ctx, buffer, bytes);
    }
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &ctx);
    fclose(file);

    char hash[33];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(hash + i * 2, "%02x", digest[i]);
    }
    hash[32] = '\0';
    return std::string(hash);
}

static bool is_directory_empty(const std::string &path) {
    DIR *dir = opendir(path.c_str());
    if (!dir) return false;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        closedir(dir);
        return false; // Найден хотя бы один элемент
    }
    closedir(dir);
    return true;
}

static void analyze_directory(const std::string &start_path, AnalysisResult *result, time_t old_threshold) {
    if (!result) {
        printf("Error: analyze_directory called with NULL result\n");
        return;
    }
    printf("Analyzing directory: %s\n", start_path.c_str());

    std::stack<std::string> dirs;
    dirs.push(start_path);

    while (!dirs.empty()) {
        std::string path = dirs.top();
        dirs.pop();

        // Проверяем, пустая ли директория
        if (is_directory_empty(path)) {
            result->empty_dirs.push_back(path);
            printf("Found empty directory: %s\n", path.c_str());
        }

        DIR *dir = opendir(path.c_str());
        if (!dir) {
            printf("Failed to open directory: %s\n", path.c_str());
            continue;
        }

        struct dirent *entry;
        struct stat st;
        char full_path[1024];

        while ((entry = readdir(dir))) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            snprintf(full_path, sizeof(full_path), "%s/%s", path.c_str(), entry->d_name);
            if (lstat(full_path, &st) == -1) {
                printf("Failed to stat: %s\n", full_path);
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                dirs.push(full_path);
            } else if (S_ISREG(st.st_mode)) {
                if (st.st_size == 0) {
                    result->empty_files.push_back(full_path);
                    printf("Found empty file: %s\n", full_path);
                }
                if (st.st_mtime < old_threshold) {
                    result->old_files.push_back(full_path);
                    printf("Found old file: %s\n", full_path);
                }
                std::string hash = compute_md5(full_path);
                if (!hash.empty()) {
                    bool found = false;
                    for (DuplicateInfo &dup : result->duplicates) {
                        if (dup.hash == hash) {
                            dup.paths.push_back(full_path);
                            printf("Found duplicate: %s (hash: %s)\n", full_path, hash.c_str());
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        DuplicateInfo dup;
                        dup.hash = hash;
                        dup.paths.push_back(full_path);
                        result->duplicates.push_back(dup);
                        printf("New hash group: %s (hash: %s)\n", full_path, hash.c_str());
                    }
                }
            }
        }
        closedir(dir);
    }
}

void analysis_perform(AppState *state, time_t old_threshold) {
    if (!state) {
        printf("Error: analysis_perform called with NULL state\n");
        return;
    }
    if (!state->current_dir) {
        printf("Error: current_dir is NULL\n");
        return;
    }
    printf("Starting analysis for directory: %s\n", state->current_dir);

    if (!state->analysis_result) {
        state->analysis_result = new AnalysisResult;
        analysis_init(state->analysis_result);
    } else {
        analysis_free(state->analysis_result);
        analysis_init(state->analysis_result);
    }

    analyze_directory(state->current_dir, state->analysis_result, old_threshold);
    state->mode = MODE_ANALYSIS;
}

Metadata analysis_get_metadata(const char *full_path) {
    Metadata meta = {};
    if (!full_path) {
        fprintf(stderr, "Error: analysis_get_metadata called with NULL path\n");
        return meta;
    }

    // Получаем метаданные файла
    struct stat st;
    if (stat(full_path, &st) == -1) {
        fprintf(stderr, "Failed to stat file: %s\n", full_path);
        return meta;
    }

    // Имя файла (берём только имя из полного пути)
    const char *filename = strrchr(full_path, '/');
    meta.name = filename ? filename + 1 : full_path;

    // Тип (директория или файл)
    meta.is_dir = S_ISDIR(st.st_mode);

    // Размер
    meta.size = st.st_size;

    // Время последнего изменения
    char mtime_str[50];
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));
    meta.mtime = mtime_str;

    // Время последнего доступа
    char atime_str[50];
    strftime(atime_str, sizeof(atime_str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_atime));
    meta.atime = atime_str;

    // Время изменения статуса
    char ctime_str[50];
    strftime(ctime_str, sizeof(ctime_str), "%Y-%m-%d %H:%M:%S", localtime(&st.st_ctime));
    meta.ctime = ctime_str;

    // Права доступа
    char permissions[11];
    snprintf(permissions, sizeof(permissions), "%c%c%c%c%c%c%c%c%c",
             (S_ISDIR(st.st_mode)) ? 'd' : '-',
             (st.st_mode & S_IRUSR) ? 'r' : '-',
             (st.st_mode & S_IWUSR) ? 'w' : '-',
             (st.st_mode & S_IXUSR) ? 'x' : '-',
             (st.st_mode & S_IRGRP) ? 'r' : '-',
             (st.st_mode & S_IWGRP) ? 'w' : '-',
             (st.st_mode & S_IXGRP) ? 'x' : '-',
             (st.st_mode & S_IROTH) ? 'r' : '-',
             (st.st_mode & S_IWOTH) ? 'w' : '-',
             (st.st_mode & S_IXOTH) ? 'x' : '-');
    meta.permissions = permissions;

    // Владелец
    struct passwd *pw = getpwuid(st.st_uid);
    meta.owner = pw ? pw->pw_name : "unknown";

    // Группа
    struct group *gr = getgrgid(st.st_gid);
    meta.group = gr ? gr->gr_name : "unknown";

    // Номер инода
    meta.inode = st.st_ino;

    return meta;
}