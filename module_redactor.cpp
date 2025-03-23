
#include "module_redactor.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace redactor {

// Функция для создания файла
bool create_file(const fs::path& file_path, const std::string& content) {
    std::ofstream file(file_path);
    if (!file) {
        std::cerr << "Ошибка: Не удалось создать файл " << file_path << std::endl;
        return false;
    }
    file << content;
    return true;
}

// Функция для чтения файла
std::string read_file(const fs::path& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        throw std::runtime_error("Ошибка: Не удалось открыть файл " + file_path.string());
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

// Функция для изменения содержимого файла
bool update_file(const fs::path& file_path, const std::string& new_content) {
    std::ofstream file(file_path, std::ios::trunc); // Открываем файл с очисткой содержимого
    if (!file) {
        std::cerr << "Ошибка: Не удалось открыть файл для изменения " << file_path << std::endl;
        return false;
    }
    file << new_content;
    return true;
}

// Функция для удаления файла
bool delete_file(const fs::path& file_path) {
    if (!fs::exists(file_path)) {
        std::cerr << "Ошибка: Файл не существует " << file_path << std::endl;
        return false;
    }
    return fs::remove(file_path);
}

// Функция для создания папки
bool create_directory(const fs::path& dir_path) {
    if (fs::exists(dir_path)) {
        std::cerr << "Ошибка: Папка уже существует " << dir_path << std::endl;
        return false;
    }
    return fs::create_directory(dir_path);
}

// Функция для удаления папки
bool delete_directory(const fs::path& dir_path) {
    if (!fs::exists(dir_path)) {
        std::cerr << "Ошибка: Папка не существует " << dir_path << std::endl;
        return false;
    }
    return fs::remove_all(dir_path) > 0; // remove_all возвращает количество удалённых файлов/папок
}

// Функция для получения списка файлов и папок в директории
std::vector<std::string> list_directory(const fs::path& dir_path) {
    std::vector<std::string> contents;
    if (!fs::exists(dir_path)) {
        std::cerr << "Ошибка: Директория не существует " << dir_path << std::endl;
        return contents;
    }
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        contents.push_back(entry.path().filename().string());
    }
    return contents;
}

} // namespace redactor


// Тестовая функция для проверки создания, чтения, изменения и удаления файла
void test_file_operations() {
    fs::path file_path = "test_file.txt";

    // Создание файла
    std::cout << "Создание файла...\n";
    if (redactor::create_file(file_path, "Привет, мир!")) {
        std::cout << "Файл успешно создан.\n";
    }

    // Чтение файла
    std::cout << "Чтение файла...\n";
    try {
        std::string content = redactor::read_file(file_path);
        std::cout << "Содержимое файла: " << content << "\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    // Изменение файла
    std::cout << "Изменение файла...\n";
    if (redactor::update_file(file_path, "Новое содержимое файла!")) {
        std::cout << "Файл успешно изменён.\n";
    }

    // Удаление файла
    std::cout << "Удаление файла...\n";
    if (redactor::delete_file(file_path)) {
        std::cout << "Файл успешно удалён.\n";
    }
}

// Тестовая функция для проверки создания и удаления папки
void test_directory_operations() {
    fs::path dir_path = "test_directory";

    // Создание папки
    std::cout << "Создание папки...\n";
    if (redactor::create_directory(dir_path)) {
        std::cout << "Папка успешно создана.\n";
    }

    // Получение списка файлов и папок
    std::cout << "Содержимое папки:\n";
    auto contents = redactor::list_directory(dir_path);
    for (const auto& item : contents) {
        std::cout << "  " << item << "\n";
    }

    // Удаление папки
    std::cout << "Удаление папки...\n";
    if (redactor::delete_directory(dir_path)) {
        std::cout << "Папка успешно удалена.\n";
    }
}

// Основная функция для тестирования
void run_tests() {
    std::cout << "=== Тестирование операций с файлами ===\n";
    test_file_operations();

    std::cout << "\n=== Тестирование операций с папками ===\n";
    test_directory_operations();
}
