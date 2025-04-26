#include "module_analization.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <map>

std::vector<FileInfo> list_directory(const std::string& dir) {
    std::vector<FileInfo> files;
    DIR* dp = opendir(dir.c_str());
    if (!dp) return files;

    struct dirent* entry;
    while ((entry = readdir(dp))) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        FileInfo info;
        if (get_file_info(dir + "/" + name, info)) {
            info.full_path = dir + "/" + name;
            files.push_back(info);
        }
    }
    closedir(dp);
    std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });
    return files;
}

bool get_file_info(const std::string& path, FileInfo& info) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    info.name = path.substr(path.find_last_of('/') + 1);
    info.is_dir = S_ISDIR(st.st_mode);
    info.size = st.st_size;
    info.mode = st.st_mode;
    info.mtime = st.st_mtime;
    info.full_path = path;
    return true;
}

std::vector<FileInfo> filter_files(const std::vector<FileInfo>& files, const std::string& pattern) {
    std::vector<FileInfo> filtered;
    for (const auto& file : files) {
        if (file.name.find(pattern) != std::string::npos) {
            filtered.push_back(file);
        }
    }
    return filtered;
}

std::vector<FileInfo> find_empty_subdirs(const std::string& dir) {
    std::vector<FileInfo> empty_dirs;
    std::vector<FileInfo> files = list_directory(dir);
    for (const auto& file : files) {
        if (file.is_dir) {
            std::string subdir = dir + "/" + file.name;
            auto subfiles = list_directory(subdir);
            if (subfiles.empty()) {
                FileInfo info = file;
                info.full_path = subdir;
                empty_dirs.push_back(info);
            } else {
                auto sub_empty = find_empty_subdirs(subdir);
                empty_dirs.insert(empty_dirs.end(), sub_empty.begin(), sub_empty.end());
            }
        }
    }
    return empty_dirs;
}

std::vector<std::vector<FileInfo>> find_duplicate_files(const std::string& dir) {
    std::vector<std::vector<FileInfo>> duplicates;
    std::map<std::string, std::vector<FileInfo>> hash_map;
    std::vector<FileInfo> files = list_directory(dir);
    
    for (const auto& file : files) {
        if (!file.is_dir) {
            std::string path = dir + "/" + file.name;
            std::ifstream in(path, std::ios::binary);
            if (!in) continue;
            std::string hash;
            char buffer[4096];
            while (in.read(buffer, sizeof(buffer))) {
                hash += std::to_string(std::hash<std::string>{}(std::string(buffer, sizeof(buffer))));
            }
            hash += std::to_string(std::hash<std::string>{}(std::string(buffer, in.gcount())));
            FileInfo info = file;
            info.full_path = path;
            hash_map[hash].push_back(info);
        }
    }
    
    for (const auto& pair : hash_map) {
        if (pair.second.size() > 1) {
            duplicates.push_back(pair.second);
        }
    }
    return duplicates;
}

std::vector<FileInfo> find_old_files(const std::string& dir, int days) {
    std::vector<FileInfo> old_files;
    time_t now = time(nullptr);
    std::vector<FileInfo> files = list_directory(dir);
    for (const auto& file : files) {
        if (!file.is_dir) {
            double seconds = difftime(now, file.mtime);
            if (seconds > days * 24 * 3600) {
                old_files.push_back(file);
            }
        }
    }
    return old_files;
}

bool change_directory(const std::string& current_dir, const std::string& target, std::string& new_dir) {
    std::string path = (target == "..") ? current_dir.substr(0, current_dir.find_last_of('/')) : current_dir + "/" + target;
    if (path.empty()) path = "/";
    char* real_path = realpath(path.c_str(), nullptr);
    if (!real_path) return false;
    new_dir = real_path;
    free(real_path);
    return true;
}