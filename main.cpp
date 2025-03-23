#include <ncursesw/ncurses.h>
#include <iostream>
#include <vector>
#include <string>
#include <locale.h>
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
size_t selected_index = 0; // Индекс выбранного элемента

// Обновление списка содержимого директории
void update_directory_contents() {
    directory_contents.clear();
    if (current_directory != "/") {
        directory_contents.push_back(".."); // Возврат в родительскую директорию
    }
    for (const auto& entry : fs::directory_iterator(current_directory)) {
        directory_contents.push_back(entry.path().filename().string());
    }
}

// Функция ввода строки с поддержкой русских букв и без лишнего пробела
std::string input_string(WINDOW* win, int y, int x, const std::string& prompt) {
    std::wstring input; // Используем wstring для поддержки широких символов (UTF-8)
    int cursor_pos = 0; // Позиция курсора в строке ввода
    curs_set(1); // Включаем курсор
    mvwprintw(win, y, x, "%s", prompt.c_str()); // Выводим подсказку
    wrefresh(win);

    wint_t ch;
    while (wget_wch(win, &ch) != ERR) {
        if (ch == '\n') { // Enter завершает ввод
            break;
        } else if (ch == KEY_BACKSPACE || ch == 127) { // Обработка Backspace
            if (cursor_pos > 0) {
                input.erase(cursor_pos - 1, 1); // Удаляем символ перед курсором
                cursor_pos--;
                // Очищаем область ввода и перерисовываем строку
                mvwprintw(win, y, x + prompt.length(), "%*s", static_cast<int>(COLS - (x + prompt.length())), " "); // Очистка всей строки
                mvwprintw(win, y, x + prompt.length(), "%ls", input.c_str()); // Выводим текущий ввод
                wmove(win, y, x + prompt.length() + cursor_pos);
            }
        } else if (ch == KEY_DC) { // Обработка Delete
            if (cursor_pos < static_cast<int>(input.length())) {
                input.erase(cursor_pos, 1); // Удаляем символ под курсором
                // Очищаем область ввода и перерисовываем строку
                mvwprintw(win, y, x + prompt.length(), "%*s", static_cast<int>(COLS - (x + prompt.length())), " "); // Очистка всей строки
                mvwprintw(win, y, x + prompt.length(), "%ls", input.c_str()); // Выводим текущий ввод
                wmove(win, y, x + prompt.length() + cursor_pos);
            }
        } else if (ch == KEY_LEFT) { // Стрелка влево
            if (cursor_pos > 0) {
                cursor_pos--;
                wmove(win, y, x + prompt.length() + cursor_pos);
            }
        } else if (ch == KEY_RIGHT) { // Стрелка вправо
            if (cursor_pos < static_cast<int>(input.length())) {
                cursor_pos++;
                wmove(win, y, x + prompt.length() + cursor_pos);
            }
        } else if (ch >= 32) { // Печатные символы, включая русские буквы
            input.insert(cursor_pos, 1, static_cast<wchar_t>(ch)); // Вставляем широкий символ
            cursor_pos++;
            // Очищаем область ввода и перерисовываем строку
            mvwprintw(win, y, x + prompt.length(), "%*s", static_cast<int>(COLS - (x + prompt.length())), " "); // Очистка всей строки
            mvwprintw(win, y, x + prompt.length(), "%ls", input.c_str()); // Выводим текущий ввод
            wmove(win, y, x + prompt.length() + cursor_pos);
        }
        wrefresh(win);
    }

    curs_set(0); // Скрываем курсор

    // Преобразуем wstring в string (UTF-8)
    std::string result;
    for (wchar_t wc : input) {
        if (wc <= 0x7F) {
            result += static_cast<char>(wc);
        } else if (wc <= 0x7FF) {
            result += static_cast<char>(0xC0 | (wc >> 6));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (wc >> 12));
            result += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
    }
    return result;
}

// Функция редактирования файла
void edit_file_content(WINDOW* main_win, const std::string& file_path) {
    // Устанавливаем локаль для поддержки UTF-8
    setlocale(LC_ALL, "");

    std::string initial_content = redactor::read_file(file_path);
    std::vector<std::wstring> lines = {L""}; // Храним строки как wstring для поддержки UTF-8
    bool is_modified = false;

    // Разбиваем начальное содержимое на строки
    std::wstring wcontent;
    for (char c : initial_content) {
        if (c == '\n') {
            lines.push_back(L"");
        } else {
            lines.back() += static_cast<wchar_t>(c);
        }
    }

    // Создаем окно для редактирования
    WINDOW* edit_win = newwin(LINES - 2, COLS - 2, 1, 1);
    box(edit_win, 0, 0);
    keypad(edit_win, TRUE);
    curs_set(1);

    int y = 0, x = 0; // Текущая позиция курсора в тексте (строка, столбец)
    int scroll_offset = 0; // Смещение для прокрутки
    int max_y = LINES - 5; // Максимальное количество строк на экране

    auto redraw = [&]() {
        wclear(edit_win);
        box(edit_win, 0, 0);
        mvwprintw(edit_win, 0, 2, "Редактор: %s", file_path.c_str());
        mvwprintw(edit_win, LINES - 3, 2, "Ctrl+X: Сохранить | Ctrl+C: Выйти | ↑/↓: Прокрутка");

        // Отображаем видимые строки
        for (int i = scroll_offset; i < static_cast<int>(lines.size()) && i - scroll_offset < max_y; i++) {
            mvwprintw(edit_win, i - scroll_offset + 1, 1, "%ls", lines[i].c_str());
        }
        wmove(edit_win, y - scroll_offset + 1, x + 1); // +1 из-за отступа от рамки
        wrefresh(edit_win);
    };

    redraw();

    wint_t ch;
    while (wget_wch(edit_win, &ch) != ERR) {
        switch (ch) {
            case KEY_BACKSPACE:
            case 127:
                if (x > 0) {
                    lines[y].erase(x - 1, 1);
                    x--;
                } else if (y > 0) {
                    x = lines[y - 1].length();
                    lines[y - 1] += lines[y];
                    lines.erase(lines.begin() + y);
                    y--;
                    if (y < scroll_offset) scroll_offset--;
                }
                is_modified = true;
                break;
            case KEY_DC: // Delete
                if (x < static_cast<int>(lines[y].length())) {
                    lines[y].erase(x, 1);
                } else if (y < static_cast<int>(lines.size()) - 1) {
                    lines[y] += lines[y + 1];
                    lines.erase(lines.begin() + y + 1);
                }
                is_modified = true;
                break;
            case '\n':
                lines.insert(lines.begin() + y + 1, lines[y].substr(x));
                lines[y] = lines[y].substr(0, x);
                y++;
                x = 0;
                if (y - scroll_offset >= max_y) scroll_offset++;
                is_modified = true;
                break;
            case KEY_UP:
                if (y > 0) {
                    y--;
                    if (x > static_cast<int>(lines[y].length())) x = lines[y].length();
                    if (y < scroll_offset) scroll_offset--;
                }
                break;
            case KEY_DOWN:
                if (y < static_cast<int>(lines.size()) - 1) {
                    y++;
                    if (x > static_cast<int>(lines[y].length())) x = lines[y].length();
                    if (y - scroll_offset >= max_y) scroll_offset++;
                }
                break;
            case KEY_LEFT:
                if (x > 0) {
                    x--;
                } else if (y > 0) {
                    y--;
                    x = lines[y].length();
                    if (y < scroll_offset) scroll_offset--;
                }
                break;
            case KEY_RIGHT:
                if (x < static_cast<int>(lines[y].length())) {
                    x++;
                } else if (y < static_cast<int>(lines.size()) - 1) {
                    y++;
                    x = 0;
                    if (y - scroll_offset >= max_y) scroll_offset++;
                }
                break;
            case 24: // Ctrl+X (Сохранить)
                {
                    std::string new_content;
                    for (const auto& line : lines) {
                        for (wchar_t wc : line) {
                            if (wc <= 0x7F) new_content += static_cast<char>(wc);
                            else if (wc <= 0x7FF) {
                                new_content += static_cast<char>(0xC0 | (wc >> 6));
                                new_content += static_cast<char>(0x80 | (wc & 0x3F));
                            } else {
                                new_content += static_cast<char>(0xE0 | (wc >> 12));
                                new_content += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
                                new_content += static_cast<char>(0x80 | (wc & 0x3F));
                            }
                        }
                        new_content += "\n";
                    }
                    if (redactor::update_file(file_path, new_content)) {
                        mvwprintw(edit_win, LINES - 2, 2, "Файл сохранен. Нажмите любую клавишу...");
                        is_modified = false;
                    } else {
                        mvwprintw(edit_win, LINES - 2, 2, "Ошибка сохранения! Нажмите любую клавишу...");
                    }
                    wrefresh(edit_win);
                    wget_wch(edit_win, &ch); // Ждем нажатия клавиши
                    redraw();
                    break;
                }
            case 3: // Ctrl+C (Выйти)
                if (is_modified) {
                    mvwprintw(edit_win, LINES - 2, 2, "Несохраненные изменения. Выйти? (y/n)");
                    wrefresh(edit_win);
                    wint_t confirm;
                    wget_wch(edit_win, &confirm);
                    if (confirm == 'y' || confirm == 'Y') {
                        delwin(edit_win);
                        curs_set(0);
                        return; // Возврат в главное меню
                    }
                    redraw();
                } else {
                    delwin(edit_win);
                    curs_set(0);
                    return; // Возврат в главное меню
                }
                break;
            default:
                if (ch >= 32) { // Печатные символы, включая русские
                    lines[y].insert(x, 1, static_cast<wchar_t>(ch));
                    x++;
                    if (x > COLS - 3) x = COLS - 3; // Ограничение по ширине окна
                    is_modified = true;
                }
                break;
        }
        redraw();
    }

    delwin(edit_win);
    curs_set(0);
}

// Основной интерфейс с полной перерисовкой
void show_main_interface(WINDOW* win) {
    wclear(win); // Очищаем окно перед отрисовкой
    box(win, 0, 0);
    update_directory_contents();
    int y = 1;

    // Закрепленные подсказки сверху
    mvwprintw(win, y++, 1, "Ctrl+A: Анализ | Ctrl+N: Новый файл | Ctrl+D: Новая папка | Q: Выход");
    mvwprintw(win, y++, 1, "↑/↓: Навигация | Enter: Открыть/Перейти | Del: Удалить");
    mvwprintw(win, y++, 1, "Текущая директория: %s", current_directory.c_str());
    mvwprintw(win, y++, 1, "Содержимое:");

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
    wrefresh(win);
}

// Функция анализа
void analyze_current_directory(WINDOW* win) {
    wclear(win); // Очищаем окно перед анализом
    box(win, 0, 0);
    int y = 1;
    mvwprintw(win, y++, 1, "Анализ папки: %s", current_directory.c_str());
    mvwprintw(win, y++, 1, "Идет анализ...");
    wrefresh(win);

    auto unused_files = find_unused_files_recursive(current_directory, 30);
    auto duplicate_files = find_duplicate_files_recursive(current_directory);
    auto empty_dirs = find_empty_directories(current_directory);

    mvwprintw(win, y++, 1, "Результаты анализа:");
    mvwprintw(win, y++, 1, "Файлы, не использованные более 30 дней:");
    for (const auto& file : unused_files) {
        mvwprintw(win, y++, 1, "  %s (Размер: %zu байт, Последнее использование: %s)",
                  file.name.c_str(), file.size, ctime(&file.last_used));
    }
    mvwprintw(win, y++, 1, "Дубликаты файлов:");
    for (const auto& group : duplicate_files) {
        for (const auto& file : group) {
            mvwprintw(win, y++, 1, "  %s", file.c_str());
        }
    }
    mvwprintw(win, y++, 1, "Пустые папки:");
    for (const auto& dir : empty_dirs) {
        mvwprintw(win, y++, 1, "  %s", dir.c_str());
    }
    mvwprintw(win, y++, 1, "Нажмите любую клавишу для возврата...");
    wrefresh(win);
    getch();
}

int main() {
    setlocale(LC_ALL, ""); // Поддержка русского языка
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    update_directory_contents();

    WINDOW* win = newwin(LINES - 1, COLS, 0, 0);
    keypad(win, TRUE); // Включаем обработку специальных клавиш для окна
    box(win, 0, 0);

    while (true) {
        show_main_interface(win);

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
                        edit_file_content(win, new_path);
                        show_main_interface(win); // Обновляем главное меню после выхода из редактора
                    }
                }
                break;
            case KEY_DC: // Delete
                {
                    std::string selected_item = directory_contents[selected_index];
                    if (selected_item != "..") {
                        fs::path item_path = current_directory + "/" + selected_item;
                        mvwprintw(win, LINES - 3, 1, "Удалить '%s'? (y/n)", selected_item.c_str());
                        wrefresh(win);
                        int confirm = getch();
                        if (confirm == 'y' || confirm == 'Y') {
                            if (fs::is_directory(item_path)) {
                                if (redactor::delete_directory(item_path)) {
                                    mvwprintw(win, LINES - 2, 1, "Папка удалена.");
                                } else {
                                    mvwprintw(win, LINES - 2, 1, "Ошибка удаления папки!");
                                }
                            } else {
                                if (redactor::delete_file(item_path)) {
                                    mvwprintw(win, LINES - 2, 1, "Файл удален.");
                                } else {
                                    mvwprintw(win, LINES - 2, 1, "Ошибка удаления файла!");
                                }
                            }
                            update_directory_contents();
                            if (selected_index >= directory_contents.size()) {
                                selected_index = directory_contents.size() - 1;
                            }
                        }
                        wrefresh(win);
                        getch(); // Очистка сообщения после подтверждения
                    }
                }
                break;
            case 1: // Ctrl+A (анализ текущей папки)
                analyze_current_directory(win);
                break;
            case 14: // Ctrl+N (новый файл)
                {
                    int y = directory_contents.size() + 5; // Учитываем строки подсказок
                    std::string filename = input_string(win, y, 1, "Имя нового файла: ");
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
            case 4: // Ctrl+D (новая папка)
                {
                    int y = directory_contents.size() + 5;
                    std::string dirname = input_string(win, y, 1, "Имя новой папки: ");
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
            case 'q':
            case 'Q':
                delwin(win);
                endwin();
                return 0;
        }
    }

    delwin(win);
    endwin();
    return 0;
}