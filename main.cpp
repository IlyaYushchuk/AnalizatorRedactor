#include <ncursesw/ncurses.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstring>
#include "module_analization.h" 
#include "module_redactor.h"    

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

// Функция ввода строки с отображением в реальном времени
std::string input_with_display(WINDOW* win, int y, int x, const std::string& prompt) {
    std::string input;
    curs_set(1); // Включаем курсор
    echo();      // Включаем отображение ввода
    mvwprintw(win, y, x, "%s", prompt.c_str());
    wrefresh(win);

    char ch;
    while ((ch = wgetch(win))) {
        if (ch == '\n') { // Enter завершает ввод
            break;
        } else if (ch == 127 || ch == KEY_BACKSPACE) { // Backspace
            if (!input.empty()) {
                input.pop_back();
                mvwprintw(win, y, x + prompt.length() + input.length(), " ");
                wmove(win, y, x + prompt.length() + input.length());
            }
        } else {
            input += ch;
        }
        mvwprintw(win, y, x + prompt.length(), "%s", input.c_str());
        wrefresh(win);
    }

    noecho();
    curs_set(0);
    return input;
}

// Функция редактирования файла (оставляем как есть, без изменений)
void edit_file_content(WINDOW* win, const std::string& file_path) {
    std::string content = redactor::read_file(file_path);
    bool is_modified = false;

    WINDOW* edit_win = newwin(LINES - 2, COLS - 2, 1, 1);
    box(edit_win, 0, 0);
    keypad(edit_win, TRUE);
    curs_set(1);

    mvwprintw(edit_win, 0, 2, "Редактор: %s", file_path.c_str());
    mvwprintw(edit_win, LINES - 3, 2, "Ctrl+S: Сохранить | Ctrl+Q: Выйти");
    wrefresh(edit_win);

    int y = 1, x = 1;
    for (char ch : content) {
        if (ch == '\n') {
            y++;
            x = 1;
        } else if (x < COLS - 2) {
            mvwaddch(edit_win, y, x++, ch);
        }
    }
    wmove(edit_win, y, x);

    int ch;
    while ((ch = wgetch(edit_win))) {
        switch (ch) {
            case KEY_BACKSPACE:
            case 127:
                if (x > 1) {
                    x--;
                    mvwaddch(edit_win, y, x, ' ');
                    wmove(edit_win, y, x);
                } else if (y > 1) {
                    y--;
                    x = COLS - 3;
                    while (mvwinch(edit_win, y, x) == ' ' && x > 1) x--;
                    x++;
                    wmove(edit_win, y, x);
                }
                is_modified = true;
                break;
            case '\n':
                y++;
                x = 1;
                wmove(edit_win, y, x);
                is_modified = true;
                break;
            case 19: // Ctrl+S
                {
                    std::string new_content;
                    for (int i = 1; i < LINES - 3; i++) {
                        char line[COLS - 2];
                        mvwinnstr(edit_win, i, 1, line, COLS - 2);
                        new_content += line;
                        new_content += '\n';
                    }
                    if (redactor::update_file(file_path, new_content)) {
                        mvwprintw(edit_win, LINES - 2, 2, "Файл сохранен.");
                        is_modified = false;
                    } else {
                        mvwprintw(edit_win, LINES - 2, 2, "Ошибка сохранения!");
                    }
                    wrefresh(edit_win);
                    break;
                }
            case 17: // Ctrl+Q
                if (is_modified) {
                    mvwprintw(edit_win, LINES - 2, 2, "Несохраненные изменения. Выйти? (y/n)");
                    wrefresh(edit_win);
                    int confirm = wgetch(edit_win);
                    if (confirm == 'y' || confirm == 'Y') {
                        goto exit_editor;
                    }
                } else {
                    goto exit_editor;
                }
                break;
            default:
                if (x < COLS - 2) {
                    mvwaddch(edit_win, y, x++, ch);
                    is_modified = true;
                }
                break;
        }
        wmove(edit_win, y, x);
        wrefresh(edit_win);
    }

exit_editor:
    delwin(edit_win);
    curs_set(0);
}


// Обновленная функция вкладки редактора
void show_editor_tab(WINDOW* win) {
    bool exit_tab = false;
    while (!exit_tab) {
        update_directory_contents(); // Обновляем содержимое текущей директории
        int y = 1;

        mvwprintw(win, y++, 1, "Редактор: %s", current_directory.c_str());
        mvwprintw(win, y++, 1, "Содержимое текущей папки:");

        // Отображение списка файлов и папок
        for (size_t i = 0; i < directory_contents.size(); ++i) {
            std::string item = directory_contents[i];
            fs::path item_path = current_directory + "/" + item;
            if (i == selected_index) {
                wattron(win, A_REVERSE); // Выделение выбранного элемента
            }
            if (item == ".." || fs::is_directory(item_path)) {
                mvwprintw(win, y++, 1, "[D] %s", item.c_str());
            } else {
                mvwprintw(win, y++, 1, "[F] %s", item.c_str());
            }
            if (i == selected_index) {
                wattroff(win, A_REVERSE);
            }
        }

        mvwprintw(win, y++, 1, "Управление:");
        mvwprintw(win, y++, 1, "↑/↓: Навигация | Enter: Открыть | Del: Удалить");
        mvwprintw(win, y++, 1, "Ctrl+D: Новая папка | Ctrl+N: Новый файл | Q: Выход");
        wrefresh(win);

        // Обработка ввода
        int ch = wgetch(win);
        switch (ch) {
            case KEY_UP:
                if (selected_index > 0) selected_index--;
                break;
            case KEY_DOWN:
                if (selected_index < directory_contents.size() - 1) selected_index++;
                break;
            case 10: // Enter
                {
                    std::string selected_item = directory_contents[selected_index];
                    std::string new_path = current_directory + "/" + selected_item;
                    if (selected_item == "..") {
                        current_directory = fs::path(current_directory).parent_path().string();
                        update_directory_contents();
                        selected_index = 0;
                    } else if (fs::is_directory(new_path)) {
                        current_directory = new_path;
                        update_directory_contents();
                        selected_index = 0;
                    } else if (fs::exists(new_path)) {
                        edit_file_content(win, new_path); // Открываем файл для редактирования
                    }
                }
                break;
            case KEY_DC: // Delete
                {
                    std::string selected_item = directory_contents[selected_index];
                    if (selected_item != "..") {
                        fs::path item_path = current_directory + "/" + selected_item;
                        mvwprintw(win, y, 1, "Удалить '%s'? (y/n)", selected_item.c_str());
                        wrefresh(win);
                        int confirm = getch();
                        if (confirm == 'y' || confirm == 'Y') {
                            if (fs::is_directory(item_path)) {
                                if (redactor::delete_directory(item_path)) {
                                    mvwprintw(win, y + 1, 1, "Папка удалена.");
                                } else {
                                    mvwprintw(win, y + 1, 1, "Ошибка удаления папки!");
                                }
                            } else {
                                if (redactor::delete_file(item_path)) {
                                    mvwprintw(win, y + 1, 1, "Файл удален.");
                                } else {
                                    mvwprintw(win, y + 1, 1, "Ошибка удаления файла!");
                                }
                            }
                            update_directory_contents();
                            if (selected_index >= directory_contents.size()) {
                                selected_index = directory_contents.size() - 1;
                            }
                        }
                        wrefresh(win);
                        getch();
                    }
                }
                break;
            case 4: // Ctrl+D (новая папка)
                {
                    std::string dirname = input_with_display(win, y, 1, "Имя новой папки: ");
                    fs::path dir_path = current_directory + "/" + dirname;
                    if (fs::exists(dir_path)) {
                        mvwprintw(win, y + 1, 1, "Ошибка: Папка '%s' уже существует!", dirname.c_str());
                    } else if (redactor::create_directory(dir_path)) {
                        mvwprintw(win, y + 1, 1, "Папка '%s' создана.", dirname.c_str());
                        update_directory_contents();
                    } else {
                        mvwprintw(win, y + 1, 1, "Ошибка создания папки!");
                    }
                    wrefresh(win);
                    getch();
                }
                break;
            case 14: // Ctrl+N (новый файл)
                {
                    std::string filename = input_with_display(win, y, 1, "Имя нового файла: ");
                    fs::path file_path = current_directory + "/" + filename;
                    if (fs::exists(file_path)) {
                        mvwprintw(win, y + 1, 1, "Ошибка: Файл '%s' уже существует!", filename.c_str());
                    } else if (redactor::create_file(file_path)) {
                        mvwprintw(win, y + 1, 1, "Файл '%s' создан.", filename.c_str());
                        update_directory_contents();
                    } else {
                        mvwprintw(win, y + 1, 1, "Ошибка создания файла!");
                    }
                    wrefresh(win);
                    getch();
                }
                break;
            case 'q':
            case 'Q':
                exit_tab = true; // Выход из вкладки редактора
                break;
        }
    }
}


void show_directory_selection_tab(WINDOW* win) {
    mvwprintw(win, 1, 1, "Выберите папку для анализа:");
    mvwprintw(win, 2, 1, "Текущая папка: %s", current_directory.c_str());
    mvwprintw(win, 3, 1, "Содержимое:");

    // Отображение содержимого директории
    int y = 5;
    for (size_t i = 0; i < directory_contents.size(); ++i) {
        std::string item = directory_contents[i];
        fs::path item_path = current_directory + "/" + item;

        if (i == selected_index) {
            wattron(win, A_REVERSE); // Выделение выбранного элемента
        }

        if (fs::is_directory(item_path)) {
            mvwprintw(win, y, 1, "[D] %s", item.c_str()); // Папка
        } else {
            mvwprintw(win, y, 1, "[F] %s", item.c_str()); // Файл
        }

        if (i == selected_index) {
            wattroff(win, A_REVERSE);
        }
        y++;
    }
    wrefresh(win);
}

// Функция для отображения вкладки анализа
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

// Функция для отображения вкладки результатов анализа
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




// Основная программа (оставляем как есть с минимальными изменениями)
int main() {
    setlocale(LC_ALL, ""); // Поддержка русского языка
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    update_directory_contents();
    int current_tab = 0;

    while (true) {
        clear();
        printw("Файловый анализатор и редактор\n");
        printw("1. Выбор папки\n2. Анализ\n3. Результаты\n4. Редактор\n5. Выход\n");
        printw("Текущая вкладка: %d\n", current_tab + 1);
        refresh();

        WINDOW* win = newwin(LINES - 5, COLS, 5, 0);
        box(win, 0, 0);
        switch (current_tab) {
            case 0: show_directory_selection_tab(win); break;
            case 1: show_analysis_tab(win); break;
            case 2: show_results_tab(win); break;
            case 3: show_editor_tab(win); break;
            case 4: endwin(); return 0;
        }
        wrefresh(win);
        delwin(win);

        int ch = getch();
        if (current_tab == 0) {
            switch (ch) {
                case KEY_UP:
                    if (selected_index > 0) selected_index--;
                    break;
                case KEY_DOWN:
                    if (selected_index < directory_contents.size() - 1) selected_index++;
                    break;
                case 10: // Enter
                    {
                        std::string selected_item = directory_contents[selected_index];
                        std::string new_path = current_directory + "/" + selected_item;
                        if (selected_item == "..") {
                            current_directory = fs::path(current_directory).parent_path().string();
                        } else if (fs::is_directory(new_path)) {
                            current_directory = new_path;
                        }
                        update_directory_contents();
                        selected_index = 0;
                    }
                    break;
            }
        }
        switch (ch) {
            case KEY_LEFT: current_tab = (current_tab - 1 + 5) % 5; break;
            case KEY_RIGHT: current_tab = (current_tab + 1) % 5; break;
            case 'q': endwin(); return 0;
            case '\t': current_tab = (current_tab + 1) % 5; break;
        }
    }

    endwin();
    return 0;
}