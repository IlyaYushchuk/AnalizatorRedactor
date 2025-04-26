#pragma once
#include <string>
#include <vector>
#include <sys/stat.h>
#include <ctime>

struct FileInfo {
    std::string name;
    std::string full_path;
    bool is_dir;
    size_t size;
    mode_t mode;   // Режим файла (из stat.st_mode)
    time_t mtime;  // Время последней модификации (из stat.st_mtime)

    bool operator==(const FileInfo& other) const {
        return name == other.name &&
               full_path == other.full_path &&
               is_dir == other.is_dir &&
               size == other.size &&
               mode == other.mode &&
               mtime == other.mtime;
    }
};

struct AnalysisResult {
    std::vector<FileInfo> empty_dirs;
    std::vector<std::vector<FileInfo>> duplicates;
    std::vector<FileInfo> old_files;
};

std::vector<FileInfo> list_directory(const std::string& dir);
bool get_file_info(const std::string& path, FileInfo& info); // Добавлено объявление
std::vector<FileInfo> filter_files(const std::vector<FileInfo>& files, const std::string& pattern);
std::vector<FileInfo> find_empty_subdirs(const std::string& dir);
std::vector<std::vector<FileInfo>> find_duplicate_files(const std::string& dir);
std::vector<FileInfo> find_old_files(const std::string& dir, int days);
bool change_directory(const std::string& current_dir, const std::string& target, std::string& new_dir);