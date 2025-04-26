#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "module_analization.h"
#include <string>
#include <vector>
#include <set>

enum class Mode {
    NAVIGATION,
    ANALYSIS,
    EDITING
};

struct ProgramState {
    bool running;
    std::string current_dir;
    std::vector<FileInfo> files;
    int selected_idx;
    std::set<size_t> selected_files;
    Mode mode;
    std::string status;
    AnalysisResult analysis_result;
    std::string filter_pattern; // Для подсветки
    std::string original_dir; // Для возврата из анализа
    std::vector<FileInfo> original_files; // Для возврата
};

ProgramState init_state();
ProgramState handle_input(const ProgramState& state, int key);

#endif