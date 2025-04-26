#include "state_manager.h"
#include "module_analization.h"
#include "module_redactor.h"
#include "UI.h"
#include <ncursesw/ncurses.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

ProgramState init_state() {
    ProgramState state;
    state.running = true;
    char* cwd = get_current_dir_name();
    state.current_dir = cwd ? cwd : "/tmp";
    if (cwd) free(cwd);
    state.files = list_directory(state.current_dir);
    state.selected_idx = 0;
    state.mode = Mode::NAVIGATION;
    state.status = "";
    state.filter_pattern = "";
    state.original_dir = state.current_dir;
    state.original_files = state.files;
    return state;
}

ProgramState handle_input(const ProgramState& state, int key) {
    ProgramState new_state = state;
    new_state.status = "";

    if (key == -1) return new_state; // Игнорируем отсутствие ввода

    if (key == 'q') {
        new_state.running = false;
        return new_state;
    }

    switch (state.mode) {
        case Mode::NAVIGATION:
            if (key == KEY_UP && new_state.selected_idx > 0) {
                new_state.selected_idx--;
            } else if (key == KEY_DOWN && new_state.selected_idx < static_cast<int>(state.files.size()) - 1) {
                new_state.selected_idx++;
            } else if (key == KEY_BACKSPACE) {
                if (!change_directory(state.current_dir, "..", new_state.current_dir)) {
                    new_state.status = "Ошибка: Не удалось вернуться назад.";
                } else {
                    new_state.files = list_directory(new_state.current_dir);
                    new_state.selected_idx = 0;
                    new_state.selected_files.clear();
                    new_state.filter_pattern = "";
                    new_state.original_dir = new_state.current_dir;
                    new_state.original_files = new_state.files;
                }
            } else if (key == KEY_ENTER || key == 10) {
                if (new_state.selected_idx >= 0 && new_state.selected_idx < static_cast<int>(state.files.size())) {
                    const FileInfo& selected = state.files[new_state.selected_idx];
                    if (selected.is_dir) {
                        if (!change_directory(state.current_dir, selected.name, new_state.current_dir)) {
                            new_state.status = "Ошибка: Не удалось перейти в директорию.";
                        } else {
                            new_state.files = list_directory(new_state.current_dir);
                            new_state.selected_idx = 0;
                            new_state.selected_files.clear();
                            new_state.filter_pattern = "";
                            new_state.original_dir = new_state.current_dir;
                            new_state.original_files = new_state.files;
                        }
                    } else {
                        std::string file_path = selected.full_path;
                        open_file_in_editor(file_path);
                        new_state.files = list_directory(new_state.current_dir);
                    }
                }
            } else if (key == 19) { // Ctrl+S
                if (new_state.selected_idx >= 0 && new_state.selected_idx < static_cast<int>(state.files.size())) {
                    size_t idx = new_state.selected_idx;
                    if (new_state.selected_files.count(idx)) {
                        new_state.selected_files.erase(idx);
                    } else {
                        new_state.selected_files.insert(idx);
                    }
                }
            } else if (key == 1) { // Ctrl+A
                new_state.mode = Mode::ANALYSIS;
                new_state.original_dir = state.current_dir;
                new_state.original_files = state.files;
                new_state.analysis_result = AnalysisResult();
                new_state.analysis_result.empty_dirs = find_empty_subdirs(state.current_dir);
                new_state.analysis_result.duplicates = find_duplicate_files(state.current_dir);
                new_state.analysis_result.old_files = find_old_files(state.current_dir, 30);
                new_state.files.clear();
                new_state.files.insert(new_state.files.end(), 
                                      new_state.analysis_result.empty_dirs.begin(), 
                                      new_state.analysis_result.empty_dirs.end());
                for (const auto& group : new_state.analysis_result.duplicates) {
                    new_state.files.insert(new_state.files.end(), group.begin(), group.end());
                }
                new_state.files.insert(new_state.files.end(), 
                                      new_state.analysis_result.old_files.begin(), 
                                      new_state.analysis_result.old_files.end());
                new_state.selected_idx = 0;
                new_state.selected_files.clear();
                new_state.status = format_analysis_result(new_state.analysis_result);
            } else if (key == 6) { // Ctrl+F
                new_state.filter_pattern = prompt_input("Введите строку для поиска: ");
                new_state.files = filter_files(list_directory(state.current_dir), new_state.filter_pattern);
                new_state.selected_idx = 0;
                new_state.selected_files.clear();
            } else if (key == 14) { // Ctrl+N
                std::string new_name = prompt_input("Имя нового файла: ");
                if (!is_valid_filename(new_name)) {
                    new_state.status = "Ошибка: Недопустимое имя.";
                } else if (!create_item(state.current_dir, new_name, false)) {
                    new_state.status = "Ошибка: Не удалось создать файл.";
                }
                new_state.files = list_directory(new_state.current_dir);
                new_state.selected_idx = 0;
                new_state.filter_pattern = "";
                new_state.original_files = new_state.files;
            } else if (key == 2) { // Ctrl+B
                std::string new_name = prompt_input("Имя новой папки: ");
                if (!is_valid_filename(new_name)) {
                    new_state.status = "Ошибка: Недопустимое имя.";
                } else if (!create_item(state.current_dir, new_name, true)) {
                    new_state.status = "Ошибка: Не удалось создать папку.";
                }
                new_state.files = list_directory(new_state.current_dir);
                new_state.selected_idx = 0;
                new_state.filter_pattern = "";
                new_state.original_files = new_state.files;
            } else if (key == 18) { // Ctrl+R
                if (new_state.selected_idx >= 0 && new_state.selected_idx < static_cast<int>(state.files.size())) {
                    std::string name = state.files[new_state.selected_idx].name;
                    std::string new_name = prompt_input("Новое имя: ");
                    if (!is_valid_filename(new_name)) {
                        new_state.status = "Ошибка: Недопустимое имя.";
                    } else if (!rename_item(state.current_dir, name, new_name)) {
                        new_state.status = "Ошибка: Не удалось переименовать.";
                    }
                    new_state.files = list_directory(new_state.current_dir);
                    new_state.selected_idx = std::min(new_state.selected_idx, static_cast<int>(new_state.files.size()) - 1);
                    new_state.filter_pattern = "";
                    new_state.original_files = new_state.files;
                }
            } else if (key == 24 || key == 22 || key == 23) { // Ctrl+X, Ctrl+V, Ctrl+W
                new_state.mode = Mode::EDITING;
            }
            break;

        case Mode::ANALYSIS:
            if (key == KEY_UP && new_state.selected_idx > 0) {
                new_state.selected_idx--;
            } else if (key == KEY_DOWN && new_state.selected_idx < static_cast<int>(state.files.size()) - 1) {
                new_state.selected_idx++;
            } else if (key == KEY_BACKSPACE) {
                new_state.current_dir = state.original_dir;
                new_state.files = state.original_files;
                new_state.mode = Mode::NAVIGATION;
                new_state.selected_idx = 0;
                new_state.selected_files.clear();
                new_state.status = "";
            } else if (key == 19) { // Ctrl+S
                if (new_state.selected_idx >= 0 && new_state.selected_idx < static_cast<int>(state.files.size())) {
                    size_t idx = new_state.selected_idx;
                    if (new_state.selected_files.count(idx)) {
                        new_state.selected_files.erase(idx);
                    } else {
                        new_state.selected_files.insert(idx);
                    }
                }
            } else if (key == 18) { // Ctrl+R
                if (new_state.selected_idx >= 0 && new_state.selected_idx < static_cast<int>(state.files.size())) {
                    std::string path = state.files[new_state.selected_idx].full_path;
                    std::string new_name = prompt_input("Новое имя: ");
                    if (!is_valid_filename(new_name)) {
                        new_state.status = "Ошибка: Недопустимое имя.";
                    } else {
                        std::string dir = path.substr(0, path.find_last_of('/'));
                        std::string old_name = path.substr(path.find_last_of('/') + 1);
                        if (!rename_item(dir, old_name, new_name)) {
                            new_state.status = "Ошибка: Не удалось переименовать.";
                        } else {
                            new_state.analysis_result = AnalysisResult();
                            new_state.analysis_result.empty_dirs = find_empty_subdirs(new_state.current_dir);
                            new_state.analysis_result.duplicates = find_duplicate_files(new_state.current_dir);
                            new_state.analysis_result.old_files = find_old_files(new_state.current_dir, 30);
                            new_state.files.clear();
                            new_state.files.insert(new_state.files.end(), 
                                                  new_state.analysis_result.empty_dirs.begin(), 
                                                  new_state.analysis_result.empty_dirs.end());
                            for (const auto& group : new_state.analysis_result.duplicates) {
                                new_state.files.insert(new_state.files.end(), group.begin(), group.end());
                            }
                            new_state.files.insert(new_state.files.end(), 
                                                  new_state.analysis_result.old_files.begin(), 
                                                  new_state.analysis_result.old_files.end());
                            new_state.selected_idx = 0;
                            new_state.selected_files.clear();
                            new_state.status = format_analysis_result(new_state.analysis_result);
                        }
                    }
                }
            } else if (key == 24) { // Ctrl+X
                if (!new_state.selected_files.empty()) {
                    std::vector<std::string> to_delete;
                    for (size_t idx : new_state.selected_files) {
                        if (idx < state.files.size()) {
                            std::string path = state.files[idx].full_path;
                            std::string name = path.substr(path.find_last_of('/') + 1);
                            std::string dir = path.substr(0, path.find_last_of('/'));
                            to_delete.push_back(name);
                            delete_item(dir, name);
                        }
                    }
                    new_state.analysis_result = AnalysisResult();
                    new_state.analysis_result.empty_dirs = find_empty_subdirs(new_state.current_dir);
                    new_state.analysis_result.duplicates = find_duplicate_files(new_state.current_dir);
                    new_state.analysis_result.old_files = find_old_files(new_state.current_dir, 30);
                    new_state.files.clear();
                    new_state.files.insert(new_state.files.end(), 
                                          new_state.analysis_result.empty_dirs.begin(), 
                                          new_state.analysis_result.empty_dirs.end());
                    for (const auto& group : new_state.analysis_result.duplicates) {
                        new_state.files.insert(new_state.files.end(), group.begin(), group.end());
                    }
                    new_state.files.insert(new_state.files.end(), 
                                          new_state.analysis_result.old_files.begin(), 
                                          new_state.analysis_result.old_files.end());
                    new_state.selected_idx = 0;
                    new_state.selected_files.clear();
                    new_state.status = format_analysis_result(new_state.analysis_result);
                } else if (new_state.selected_idx >= 0 && new_state.selected_idx < static_cast<int>(state.files.size())) {
                    std::string path = state.files[new_state.selected_idx].full_path;
                    std::string name = path.substr(path.find_last_of('/') + 1);
                    std::string dir = path.substr(0, path.find_last_of('/'));
                    if (!delete_item(dir, name)) {
                        new_state.status = "Ошибка: Не удалось удалить.";
                    } else {
                        new_state.analysis_result = AnalysisResult();
                        new_state.analysis_result.empty_dirs = find_empty_subdirs(new_state.current_dir);
                        new_state.analysis_result.duplicates = find_duplicate_files(new_state.current_dir);
                        new_state.analysis_result.old_files = find_old_files(new_state.current_dir, 30);
                        new_state.files.clear();
                        new_state.files.insert(new_state.files.end(), 
                                              new_state.analysis_result.empty_dirs.begin(), 
                                              new_state.analysis_result.empty_dirs.end());
                        for (const auto& group : new_state.analysis_result.duplicates) {
                            new_state.files.insert(new_state.files.end(), group.begin(), group.end());
                        }
                        new_state.files.insert(new_state.files.end(), 
                                              new_state.analysis_result.old_files.begin(), 
                                              new_state.analysis_result.old_files.end());
                        new_state.selected_idx = 0;
                        new_state.status = format_analysis_result(new_state.analysis_result);
                    }
                }
            }
            break;

        case Mode::EDITING:
            if (key == 24 || key == 22 || key == 23) { // Ctrl+X, Ctrl+V, Ctrl+W
                if (key == 24) { // Ctrl+X
                    if (!new_state.selected_files.empty()) {
                        std::vector<std::string> to_delete;
                        for (size_t idx : new_state.selected_files) {
                            if (idx < state.files.size()) {
                                to_delete.push_back(state.files[idx].name);
                            }
                        }
                        batch_delete(state.current_dir, to_delete);
                        new_state.selected_files.clear();
                    } else if (new_state.selected_idx >= 0 && new_state.selected_idx < static_cast<int>(state.files.size())) {
                        std::string name = state.files[new_state.selected_idx].name;
                        if (!delete_item(state.current_dir, name)) {
                            new_state.status = "Ошибка: Не удалось удалить.";
                        }
                    }
                } else if (key == 22) { // Ctrl+V
                    if (!new_state.selected_files.empty()) {
                        std::vector<std::string> to_copy;
                        for (size_t idx : new_state.selected_files) {
                            if (idx < state.files.size()) {
                                to_copy.push_back(state.files[idx].name);
                            }
                        }
                        std::string prefix = prompt_input("Префикс для копий: ");
                        batch_copy(state.current_dir, to_copy, prefix);
                    } else {
                        new_state.status = "Ошибка: Выделите файлы.";
                    }
                } else if (key == 23) { // Ctrl+W
                    if (!new_state.selected_files.empty()) {
                        std::vector<std::string> to_move;
                        for (size_t idx : new_state.selected_files) {
                            if (idx < state.files.size()) {
                                to_move.push_back(state.files[idx].name);
                            }
                        }
                        std::string prefix = prompt_input("Префикс для перемещения: ");
                        batch_move(state.current_dir, to_move, prefix);
                        new_state.selected_files.clear();
                    } else {
                        new_state.status = "Ошибка: Выделите файлы.";
                    }
                }
                new_state.files = list_directory(new_state.current_dir);
                new_state.selected_idx = std::min(new_state.selected_idx, static_cast<int>(new_state.files.size()) - 1);
                new_state.filter_pattern = "";
                new_state.original_files = new_state.files;
            }
            new_state.mode = Mode::NAVIGATION;
            break;
    }

    return new_state;
}