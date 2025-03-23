#include "module_analization.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <algorithm>
#include <fstream>


namespace fs = std::filesystem;

// Функция для вычисления хэша файла по его содержимому
std::string calculate_file_hash(const fs::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Не удалось открыть файл: " + file_path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return std::to_string(std::hash<std::string>{}(content));
}

// Функция для поиска файлов с одинаковым содержимым
std::vector<std::vector<fs::path>> find_duplicate_files(const fs::path& directory) {
    std::unordered_map<std::string, std::vector<fs::path>> hash_to_files;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry)) {
            std::string file_hash = calculate_file_hash(entry.path());
            hash_to_files[file_hash].push_back(entry.path());
        }
    }

    std::vector<std::vector<fs::path>> duplicates;
    for (const auto& [hash, files] : hash_to_files) {
        if (files.size() > 1) {
            duplicates.push_back(files);
        }
    }

    return duplicates;
}

// Функция для поиска пустых папок
std::vector<fs::path> find_empty_directories(const fs::path& directory) {
    std::vector<fs::path> empty_dirs;

    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (fs::is_directory(entry) && fs::is_empty(entry)) {
            empty_dirs.push_back(entry.path());
        }
    }

    return empty_dirs;
}
// Функция для преобразования file_time_type в system_clock::time_point
std::chrono::system_clock::time_point to_system_time(const fs::file_time_type& ft) {
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
}
// Функция для преобразования system_clock::time_point в file_time_type
fs::file_time_type to_file_time(const std::chrono::system_clock::time_point& tp) {
    // Разница между system_clock и file_time_type::clock
    auto system_now = std::chrono::system_clock::now();
    auto file_now = fs::file_time_type::clock::now();
    auto diff = file_now - fs::file_time_type::clock::now();

    // Преобразуем system_clock::time_point в file_time_type
    return fs::file_time_type::clock::now() + (tp - system_now);
}

// Функция для поиска файлов, которые давно не использовались
std::vector<fs::path> find_unused_files(const fs::path& directory, int days_threshold = 30) {
    std::vector<fs::path> unused_files;
    auto now = std::chrono::system_clock::now();

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (fs::is_regular_file(entry)) {
            auto last_used_time = fs::last_write_time(entry);
            auto last_used_system_time = std::chrono::system_clock::now() - (fs::file_time_type::clock::now() - last_used_time);
            auto last_used_duration = std::chrono::duration_cast<std::chrono::hours>(now - last_used_system_time).count() / 24;

            if (last_used_duration > days_threshold) {
                unused_files.push_back(entry.path());
            }
        }
    }

    return unused_files;
}

// Функция для изменения времени последнего изменения файла
void set_file_last_write_time(const fs::path& file_path, int days_ago) {
    auto now = std::chrono::system_clock::now();
    auto new_time = now - std::chrono::hours(24 * days_ago);
    fs::last_write_time(file_path, to_file_time(new_time));
}

// Тесты для проверки функций
void run_tests() {
    // // Создаем временную директорию для тестов
    fs::path test_dir = "/home/ilya/Рабочий стол/Cursach/test_dir";
    // fs::create_directories(test_dir);

    // // Создаем тестовые файлы
    // fs::path file1 = test_dir / "file1.txt";
    // fs::path file2 = test_dir / "file2.txt";
    // fs::path file3 = test_dir / "file3.txt";
    // fs::path empty_dir1 = test_dir / "empty_dir1";
    // fs::path empty_dir2 = test_dir / "empty_dir2";

    // std::ofstream(file1) << "Hello, World!";
    // std::ofstream(file2) << "Hello, World!";
    // std::ofstream(file3) << "Different content";
    // fs::create_directories(empty_dir1);
    // fs::create_directories(empty_dir2);

    // // Устанавливаем старую дату последнего изменения для file1 и file2
    // set_file_last_write_time(file1, 40); // 40 дней назад
    // set_file_last_write_time(file2, 35); // 35 дней назад
    // set_file_last_write_time(file3, 10); // 10 дней назад

    // Тест для поиска файлов, которые давно не использовались
    auto unused_files = find_unused_files(test_dir, 30); // Порог 30 дней
    std::cout << "Файлы, которые не использовались более 30 дней:\n";
    for (const auto& file : unused_files) {
        std::cout << file << "\n";
    }

    // Тест для поиска файлов с одинаковым содержимым
    auto duplicate_files = find_duplicate_files(test_dir);
    std::cout << "\nФайлы с одинаковым содержимым:\n";
    for (const auto& group : duplicate_files) {
        for (const auto& file : group) {
            std::cout << file << "\n";
        }
        std::cout << "----\n";
    }

    // Тест для поиска пустых папок
    auto empty_dirs = find_empty_directories(test_dir);
    std::cout << "\nПустые папки:\n";
    for (const auto& dir : empty_dirs) {
        std::cout << dir << "\n";
    }

    // Удаляем временную директорию после тестов
    // fs::remove_all(test_dir);
}

int main() {
    run_tests();
    return 0;
}