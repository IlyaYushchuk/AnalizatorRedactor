#ifndef MODULE_REDACTOR_H
#define MODULE_REDACTOR_H

#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace redactor {

// Функция для создания файла
bool create_file(const fs::path& file_path, const std::string& content = "");

// Функция для чтения файла
std::string read_file(const fs::path& file_path);

// Функция для изменения содержимого файла
bool update_file(const fs::path& file_path, const std::string& new_content);

// Функция для удаления файла
bool delete_file(const fs::path& file_path);

// Функция для создания папки
bool create_directory(const fs::path& dir_path);

// Функция для удаления папки
bool delete_directory(const fs::path& dir_path);

// Функция для получения списка файлов и папок в директории
std::vector<std::string> list_directory(const fs::path& dir_path);

} // namespace redactor

#endif // MODULE_REDACTOR_H