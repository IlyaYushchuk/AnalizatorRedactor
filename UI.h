#ifndef UI_H
#define UI_H

#include "module_analization.h"
#include "state_manager.h"

bool init_ui();
void cleanup_ui();
void render_ui(const ProgramState& state);
int get_user_input();
std::string prompt_input(const std::string& prompt);
std::string format_file_info(const FileInfo& file);
std::string format_analysis_result(const AnalysisResult& result);
void open_file_in_editor(const std::string& path);
void batch_delete(const std::string& dir, const std::vector<std::string>& names);
void batch_copy(const std::string& dir, const std::vector<std::string>& src_names, const std::string& dest_prefix);
void batch_move(const std::string& dir, const std::vector<std::string>& src_names, const std::string& dest_prefix);

#endif