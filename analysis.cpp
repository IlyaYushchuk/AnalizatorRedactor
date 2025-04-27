#include "analysis.h"
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <stack>
#include <unistd.h>

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

void analysis_draw(AppState *state) {
    if (!state || !state->analysis_result) {
        printf("Error: analysis_draw called with NULL state or result\n");
        clear();
        mvprintw(0, 0, "Analysis failed: No results");
        refresh();
        return;
    }

    clear();
    unsigned int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int y = 0;

    mvprintw(y++, 0, "Analysis of directory: %s", state->current_dir);

    size_t total_items = state->analysis_result->old_files.size() +
                        state->analysis_result->empty_files.size() +
                        state->analysis_result->empty_dirs.size();
    for (const auto &dup : state->analysis_result->duplicates) {
        if (dup.paths.size() > 1) total_items += dup.paths.size();
    }

    y++;
    mvprintw(y++, 0, "Old files:");
    size_t item_index = 0;
    for (size_t i = 0; i < state->analysis_result->old_files.size(); i++) {
        if (y >= max_y - 2) break;
        bool selected = (state->analysis_result->section == AnalysisResult::SECTION_OLD &&
                         state->analysis_result->selected_index == item_index);
        if (selected) attron(COLOR_PAIR(1));
        mvprintw(y++, 0, "  %s", state->analysis_result->old_files[i].c_str());
        if (selected) attroff(COLOR_PAIR(1));
        item_index++;
    }

    y++;
    mvprintw(y++, 0, "Empty files:");
    for (size_t i = 0; i < state->analysis_result->empty_files.size(); i++) {
        if (y >= max_y - 2) break;
        bool selected = (state->analysis_result->section == AnalysisResult::SECTION_EMPTY_FILES &&
                         state->analysis_result->selected_index == item_index);
        if (selected) attron(COLOR_PAIR(1));
        mvprintw(y++, 0, "  %s", state->analysis_result->empty_files[i].c_str());
        if (selected) attroff(COLOR_PAIR(1));
        item_index++;
    }

    y++;
    mvprintw(y++, 0, "Empty directories:");
    for (size_t i = 0; i < state->analysis_result->empty_dirs.size(); i++) {
        if (y >= max_y - 2) break;
        bool selected = (state->analysis_result->section == AnalysisResult::SECTION_EMPTY_DIRS &&
                         state->analysis_result->selected_index == item_index);
        if (selected) attron(COLOR_PAIR(1));
        mvprintw(y++, 0, "  %s", state->analysis_result->empty_dirs[i].c_str());
        if (selected) attroff(COLOR_PAIR(1));
        item_index++;
    }

    y++;
    mvprintw(y++, 0, "Duplicate files:");
    for (const DuplicateInfo &dup : state->analysis_result->duplicates) {
        if (dup.paths.size() > 1) {
            if (y >= max_y - 2) break;
            mvprintw(y++, 0, "  Hash: %s", dup.hash.c_str());
            for (size_t i = 0; i < dup.paths.size(); i++) {
                if (y >= max_y - 2) break;
                bool selected = (state->analysis_result->section == AnalysisResult::SECTION_DUPLICATES &&
                                 state->analysis_result->selected_index == item_index);
                if (selected) attron(COLOR_PAIR(1));
                mvprintw(y++, 0, "    %s", dup.paths[i].c_str());
                if (selected) attroff(COLOR_PAIR(1));
                item_index++;
            }
        }
    }

    mvprintw(max_y - 1, 0, "q: Quit | Esc: Back | Arrows: Navigate | Enter: Open | F4: Delete");
    refresh();
}

int analysis_handle_input(AppState *state) {
    if (!state || !state->analysis_result) {
        printf("Error: analysis_handle_input called with NULL state or result\n");
        return 0;
    }

    int ch = getch();
    AnalysisResult *result = state->analysis_result;

    size_t total_items = result->old_files.size() + result->empty_files.size() + result->empty_dirs.size();
    for (const auto &dup : result->duplicates) {
        if (dup.paths.size() > 1) total_items += dup.paths.size();
    }

    switch (ch) {
        case 'q':
            return 0;
        case 27: // Esc
            state->mode = MODE_BROWSE;
            break;
        case KEY_UP:
            if (result->selected_index > 0) {
                result->selected_index--;
                // Обновляем секцию
                size_t idx = 0;
                if (result->selected_index < result->old_files.size()) {
                    result->section = AnalysisResult::SECTION_OLD;
                } else if (result->selected_index < result->old_files.size() + result->empty_files.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_FILES;
                } else if (result->selected_index < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_DIRS;
                } else {
                    result->section = AnalysisResult::SECTION_DUPLICATES;
                }
            }
            break;
        case KEY_DOWN:
            if (result->selected_index + 1 < total_items) {
                result->selected_index++;
                // Обновляем секцию
                size_t idx = 0;
                if (result->selected_index < result->old_files.size()) {
                    result->section = AnalysisResult::SECTION_OLD;
                } else if (result->selected_index < result->old_files.size() + result->empty_files.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_FILES;
                } else if (result->selected_index < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    result->section = AnalysisResult::SECTION_EMPTY_DIRS;
                } else {
                    result->section = AnalysisResult::SECTION_DUPLICATES;
                }
            }
            break;
        case '\n': // Enter
            {
                std::string selected_path;
                size_t idx = result->selected_index;
                if (idx < result->old_files.size()) {
                    selected_path = result->old_files[idx];
                } else if (idx < result->old_files.size() + result->empty_files.size()) {
                    selected_path = result->empty_files[idx - result->old_files.size()];
                } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    // Пустые директории не открываем в редакторе
                    break;
                } else {
                    size_t dup_idx = idx - (result->old_files.size() + result->empty_files.size() + result->empty_dirs.size());
                    size_t current = 0;
                    for (const auto &dup : result->duplicates) {
                        if (dup.paths.size() > 1) {
                            for (size_t i = 0; i < dup.paths.size(); i++) {
                                if (current == dup_idx) {
                                    selected_path = dup.paths[i];
                                    break;
                                }
                                current++;
                            }
                        }
                        if (!selected_path.empty()) break;
                    }
                }
                if (!selected_path.empty()) {
                    state_set_edit_file(state, selected_path.c_str());
                    state->mode = MODE_EDITOR;
                }
            }
            break;
        case KEY_F(4): // F4 для удаления
            {
                std::string selected_path;
                size_t idx = result->selected_index;
                bool is_dir = false;
                if (idx < result->old_files.size()) {
                    selected_path = result->old_files[idx];
                } else if (idx < result->old_files.size() + result->empty_files.size()) {
                    selected_path = result->empty_files[idx - result->old_files.size()];
                } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                    selected_path = result->empty_dirs[idx - (result->old_files.size() + result->empty_files.size())];
                    is_dir = true;
                } else {
                    size_t dup_idx = idx - (result->old_files.size() + result->empty_files.size() + result->empty_dirs.size());
                    size_t current = 0;
                    for (const auto &dup : result->duplicates) {
                        if (dup.paths.size() > 1) {
                            for (size_t i = 0; i < dup.paths.size(); i++) {
                                if (current == dup_idx) {
                                    selected_path = dup.paths[i];
                                    break;
                                }
                                current++;
                            }
                        }
                        if (!selected_path.empty()) break;
                    }
                }
                if (!selected_path.empty()) {
                    mvprintw(0, 0, "Delete %s? (y/n)", selected_path.c_str());
                    refresh();
                    int confirm = getch();
                    if (confirm == 'y' || confirm == 'Y') {
                        int ret = is_dir ? rmdir(selected_path.c_str()) : unlink(selected_path.c_str());
                        if (ret == 0) {
                            printf("Deleted %s: %s\n", is_dir ? "directory" : "file", selected_path.c_str());
                            // Удаляем из результатов
                            if (idx < result->old_files.size()) {
                                result->old_files.erase(result->old_files.begin() + idx);
                            } else if (idx < result->old_files.size() + result->empty_files.size()) {
                                result->empty_files.erase(result->empty_files.begin() + (idx - result->old_files.size()));
                            } else if (idx < result->old_files.size() + result->empty_files.size() + result->empty_dirs.size()) {
                                result->empty_dirs.erase(result->empty_dirs.begin() + (idx - (result->old_files.size() + result->empty_files.size())));
                            } else {
                                size_t dup_idx = idx - (result->old_files.size() + result->empty_files.size() + result->empty_dirs.size());
                                size_t current = 0;
                                for (auto &dup : result->duplicates) {
                                    if (dup.paths.size() > 1) {
                                        for (size_t i = 0; i < dup.paths.size(); i++) {
                                            if (current == dup_idx) {
                                                dup.paths.erase(dup.paths.begin() + i);
                                                break;
                                            }
                                            current++;
                                        }
                                    }
                                }
                            }
                            if (result->selected_index > 0 && result->selected_index >= total_items - 1) {
                                result->selected_index--;
                            }
                        } else {
                            printf("Failed to delete %s: %s\n", is_dir ? "directory" : "file", selected_path.c_str());
                        }
                    }
                }
            }
            break;
    }
    return 1;
}