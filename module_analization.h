#ifndef MODULE_ANALIZATION_H
#define MODULE_ANALIZATION_H

#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Структура для хранения информации о файле
struct FileInfo {
    std::string name;
    std::string path;
    std::size_t size;
    std::time_t last_used;
};

// Функция для поиска файлов, которые давно не использовались
std::vector<FileInfo> find_unused_files(const fs::path& directory, int days_threshold = 30);

// Функция для поиска файлов с одинаковым содержимым
std::vector<std::vector<fs::path>> find_duplicate_files(const fs::path& directory);

// Функция для поиска пустых папок
std::vector<fs::path> find_empty_directories(const fs::path& directory);

#endif // MODULE_ANALIZATION_H