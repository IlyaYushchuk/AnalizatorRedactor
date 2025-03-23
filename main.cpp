#include <ncursesw/ncurses.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstring>
#include "module_analization.h"

namespace fs = std::filesystem;

// Глобальные переменные
std::string current_directory = fs::current_path().string();
std::vector<std::string> directory_contents; // Содержимое текущей директории
size_t selected_index = 0; // Индекс выбранного элемента в списке

// Функция для обновления списка содержимого директории
void update_directory_contents() {
    directory_contents.clear();
    if (current_directory != "/") {
        directory_contents.push_back(".."); // Добавляем возможность вернуться назад
    }
    for (const auto& entry : fs::directory_iterator(current_directory)) {
        directory_contents.push_back(entry.path().filename().string());
    }
}

// Функция для отображения вкладки выбора папки
void show_directory_selection_tab(WINDOW* win) {
    mvwprintw(win, 1, 1, "Выберите папку для анализа:");
    mvwprintw(win, 2, 1, "Текущая папка: %s", current_directory.c_str());
    mvwprintw(win, 3, 1, "Содержимое:");

    // Отображение содержимого директории
    int y = 5;
    for (size_t i = 0; i < directory_contents.size(); ++i) {
        if (i == selected_index) {
            wattron(win, A_REVERSE); // Выделение выбранного элемента
        }
        mvwprintw(win, y++, 1, "%s", directory_contents[i].c_str());
        if (i == selected_index) {
            wattroff(win, A_REVERSE);
        }
    }
    wrefresh(win);
}

void show_analysis_tab(WINDOW* win) {
    mvwprintw(win, 1, 1, "Анализ папки: %s", current_directory.c_str());
    mvwprintw(win, 2, 1, "Идет анализ...");
    wrefresh(win);

    // Вызов рекурсивных функций анализа
    auto unused_files = find_unused_files_recursive(current_directory, 30);
    auto duplicate_files = find_duplicate_files_recursive(current_directory);
    auto empty_dirs = find_empty_directories(current_directory);

    mvwprintw(win, 2, 1, "Анализ завершен. Нажмите любую клавишу для продолжения.");
    wrefresh(win);
    getch();
}

void show_results_tab(WINDOW* win) {
    mvwprintw(win, 1, 1, "Результаты анализа:");
    int y = 3;

    // Отображение давно не использованных файлов
    auto unused_files = find_unused_files_recursive(current_directory, 30);
    mvwprintw(win, y++, 1, "Файлы, которые не использовались более 30 дней:");
    for (const auto& file : unused_files) {
        mvwprintw(win, y++, 1, "  Файл: %s, Размер: %zu байт, Последнее использование: %s",
                 file.name.c_str(), file.size, ctime(&file.last_used));
    }

    // Отображение дубликатов файлов
    auto duplicate_files = find_duplicate_files_recursive(current_directory);
    mvwprintw(win, y++, 1, "Файлы с одинаковым содержимым:");
    for (const auto& group : duplicate_files) {
        for (const auto& file : group) {
            mvwprintw(win, y++, 1, "  Файл: %s", file.c_str());
        }
    }

    // Отображение пустых папок
    auto empty_dirs = find_empty_directories(current_directory);
    mvwprintw(win, y++, 1, "Пустые папки:");
    for (const auto& dir : empty_dirs) {
        mvwprintw(win, y++, 1, "  Папка: %s", dir.c_str());
    }

    wrefresh(win);
}

// Функция для отображения вкладки редактора
void show_editor_tab(WINDOW* win) {
    mvwprintw(win, 1, 1, "Редактор файлов:");
    int y = 3;
    auto unused_files = find_unused_files_recursive(current_directory, 30);
    for (size_t i = 0; i < unused_files.size(); ++i) {
        mvwprintw(win, y, 1, "%zu. Файл: %s", i + 1, unused_files[i].name.c_str());
        y++;
    }
    mvwprintw(win, y + 1, 1, "Введите номер файла для удаления или 0 для выхода:");
    wrefresh(win);

    char input[10];
    echo();
    curs_set(1);
    mvwgetnstr(win, y + 2, 1, input, sizeof(input) - 1);
    curs_set(0);
    noecho();

    size_t choice = atoi(input);
    if (choice > 0 && choice <= unused_files.size()) {
        fs::remove(unused_files[choice - 1].path);
    }
}

int main() {
    // Инициализация ncurses
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Обновление списка содержимого текущей директории
    update_directory_contents();

    // Основной цикл программы
    int current_tab = 0;
    while (true) {
        clear();
        printw("Файловый анализатор и редактор\n");
        printw("1. Выбор папки\n2. Анализ\n3. Результаты\n4. Редактор\n5. Выход\n");
        printw("Текущая вкладка: %d\n", current_tab + 1);
        refresh();

        // Отображение текущей вкладки
        WINDOW* win = newwin(LINES - 5, COLS, 5, 0);
        box(win, 0, 0);
        switch (current_tab) {
            case 0:
                show_directory_selection_tab(win);
                break;
            case 1:
                show_analysis_tab(win);
                break;
            case 2:
                show_results_tab(win);
                break;
            case 3:
                show_editor_tab(win);
                break;
            case 4:
                endwin();
                return 0;
        }
        wrefresh(win);
        delwin(win);

        // Обработка ввода пользователя
        int ch = getch();
        if (current_tab == 0) {
            // Навигация по папкам
            switch (ch) {
                case KEY_UP:
                    if (selected_index > 0) {
                        selected_index--;
                    }
                    break;
                case KEY_DOWN:
                    if (selected_index < directory_contents.size() - 1) {
                        selected_index++;
                    }
                    break;
                case 10: // Enter
                    {
                        std::string selected_item = directory_contents[selected_index];
                        std::string new_path = current_directory + "/" + selected_item;
                        if (selected_item == "..") {
                            // Возврат в родительскую директорию
                            current_directory = fs::path(current_directory).parent_path().string();
                        } else if (fs::is_directory(new_path)) {
                            current_directory = new_path;
                        }
                        update_directory_contents();
                        selected_index = 0;
                    }
                    break;
                case KEY_BACKSPACE:
                case 127: // Backspace
                    // Возврат в родительскую директорию
                    if (current_directory != "/") {
                        current_directory = fs::path(current_directory).parent_path().string();
                        update_directory_contents();
                        selected_index = 0;
                    }
                    break;
            }
        }

        // Переключение между вкладками
        switch (ch) {
            case KEY_LEFT:
                current_tab = (current_tab - 1 + 5) % 5;
                break;
            case KEY_RIGHT:
                current_tab = (current_tab + 1) % 5;
                break;
            case 'q':
                endwin();
                return 0;
            case '\t': // Tab
                current_tab = (current_tab + 1) % 5;
                break;
        }
    }

    endwin();
    return 0;
}