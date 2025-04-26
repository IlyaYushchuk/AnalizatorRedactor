#include "module_redactor.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <errno.h>

bool create_item(const std::string& dir, const std::string& name, bool is_dir) {
    std::string path = dir + "/" + name;
    if (is_dir) {
        return mkdir(path.c_str(), 0755) == 0;
    } else {
        std::ofstream file(path);
        return file.good();
    }
}

bool rename_item(const std::string& dir, const std::string& old_name, const std::string& new_name) {
    std::string old_path = dir + "/" + old_name;
    std::string new_path = dir + "/" + new_name;
    return rename(old_path.c_str(), new_path.c_str()) == 0;
}

bool delete_item(const std::string& dir, const std::string& name) {
    std::string path = dir + "/" + name;
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    if (S_ISDIR(st.st_mode)) {
        return rmdir(path.c_str()) == 0;
    } else {
        return unlink(path.c_str()) == 0;
    }
}

bool copy_file(const std::string& dir, const std::string& src_name, const std::string& dest_name) {
    std::string src_path = dir + "/" + src_name;
    std::string dest_path = dir + "/" + dest_name;
    std::ifstream src(src_path, std::ios::binary);
    std::ofstream dest(dest_path, std::ios::binary);
    if (!src || !dest) return false;
    dest << src.rdbuf();
    return src.good() && dest.good();
}

bool move_file(const std::string& dir, const std::string& src_name, const std::string& dest_name) {
    if (rename_item(dir, src_name, dest_name)) return true;
    if (errno != EXDEV) return false;
    if (!copy_file(dir, src_name, dest_name)) return false;
    return delete_item(dir, src_name);
}

bool is_valid_filename(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos) {
        return false;
    }
    for (char c : name) {
        if (c == '\0' || c == '\\') return false;
    }
    return true;
}