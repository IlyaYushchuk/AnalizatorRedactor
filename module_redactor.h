#ifndef MODULE_REDACTOR_H
#define MODULE_REDACTOR_H

#include <string>

bool create_item(const std::string& dir, const std::string& name, bool is_dir);
bool rename_item(const std::string& dir, const std::string& old_name, const std::string& new_name);
bool delete_item(const std::string& dir, const std::string& name);
bool copy_file(const std::string& dir, const std::string& src_name, const std::string& dest_name);
bool move_file(const std::string& dir, const std::string& src_name, const std::string& dest_name);
bool is_valid_filename(const std::string& name);

#endif