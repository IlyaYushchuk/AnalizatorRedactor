#include "UI.h"
#include <ncursesw/ncurses.h>
#include <locale.h>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include "module_redactor.h"

bool init_ui() {
    setlocale(LC_ALL, "");
    initscr();
    if (!has_colors()) {
        endwin();
        return false;
    }
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLUE, -1); // Папки
    init_pair(2, COLOR_WHITE, -1); // Файлы
    init_pair(3, COLOR_RED, -1); // Ошибки
    init_pair(4, COLOR_GREEN, -1); // Выделенные
    init_pair(5, COLOR_YELLOW, -1); // Подстрока фильтра
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    return true;
}

void cleanup_ui() {
    endwin();
}

void render_ui(const ProgramState& state) {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    mvprintw(0, 0, "Текущая директория: %s", state.current_dir.c_str());
    mvprintw(1, 0, "Режим: %s", state.mode == Mode::NAVIGATION ? "Навигация" :
                             state.mode == Mode::ANALYSIS ? "Анализ" : "Редактирование");

    int display_lines = max_y - 7;
    for (size_t i = 0; i < state.files.size() && i < static_cast<size_t>(display_lines); ++i) {
        bool is_selected = (i == static_cast<size_t>(state.selected_idx));
        bool is_highlighted = state.selected_files.find(i) != state.selected_files.end();
        if (is_selected) attron(A_REVERSE);
        if (is_highlighted) attron(COLOR_PAIR(4));
        
        std::string display_name = state.files[i].name;
        if (state.mode == Mode::ANALYSIS) {
            display_name = state.files[i].full_path; // Показываем полный путь в анализе
        }

        if (state.files[i].is_dir) {
            attron(COLOR_PAIR(1));
            mvprintw(i + 2, 0, "[DIR] ");
            attroff(COLOR_PAIR(1));
        } else {
            attron(COLOR_PAIR(2));
            mvprintw(i + 2, 0, "[FILE] ");
            attroff(COLOR_PAIR(2));
        }

        // Подсветка подстроки
        if (!state.filter_pattern.empty() && state.mode != Mode::ANALYSIS) {
            size_t pos = display_name.find(state.filter_pattern);
            if (pos != std::string::npos) {
                std::string before = display_name.substr(0, pos);
                std::string match = display_name.substr(pos, state.filter_pattern.length());
                std::string after = display_name.substr(pos + state.filter_pattern.length());
                printw("%s", before.c_str());
                attron(COLOR_PAIR(5));
                printw("%s", match.c_str());
                attroff(COLOR_PAIR(5));
                printw("%s", after.c_str());
            } else {
                printw("%s", display_name.c_str());
            }
        } else {
            printw("%s", display_name.c_str());
        }

        if (is_highlighted) attroff(COLOR_PAIR(4));
        if (is_selected) attroff(A_REVERSE);
    }

    if (!state.status.empty()) {
        attron(COLOR_PAIR(3));
        mvprintw(max_y - 5, 0, "%s", state.status.c_str());
        attroff(COLOR_PAIR(3));
    }

    mvprintw(max_y - 4, 0, "Ctrl+S - выделить, Ctrl+A - анализ, Ctrl+N - новый файл, Ctrl+B - новая папка,");
    mvprintw(max_y - 3, 0, "Ctrl+R - переименовать, Ctrl+X - удалить выделенные, Ctrl+V - копировать выделенные,");
    mvprintw(max_y - 2, 0, "Ctrl+W - вырезать выделенные, Ctrl+F - фильтр, Enter - открыть, Backspace - назад, q - выход");
    refresh();
}

int get_user_input() {
    timeout(100); // Уменьшаем задержку для отзывчивости
    int ch = getch();
    if (ch == ERR) return -1; // Игнорируем отсутствие ввода
    return ch;
}

std::string prompt_input(const std::string& prompt) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(max_y - 5, 0, "%s", prompt.c_str());
    clrtoeol();
    echo();
    curs_set(1);
    char input[256];
    getstr(input);
    noecho();
    curs_set(0);
    return std::string(input);
}

std::string format_file_info(const FileInfo& file) {
    std::stringstream ss;
    ss << file.name << " (" << file.size << " Б)";
    return ss.str();
}

std::string format_analysis_result(const AnalysisResult& result) {
    std::stringstream ss;
    ss << "Результаты анализа:\n";
    ss << "Пустые подпапки: " << result.empty_dirs.size() << "\n";
    ss << "Дубликаты: " << result.duplicates.size() << " групп\n";
    ss << "Старые файлы: " << result.old_files.size() << "\n";
    return ss.str();
}

void open_file_in_editor(const std::string& path) {
    cleanup_ui();
    pid_t pid = fork();
    if (pid == 0) {
        execlp("nano", "nano", path.c_str(), nullptr);
        exit(1);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
    init_ui();
}

void batch_delete(const std::string& dir, const std::vector<std::string>& names) {
    for (const auto& name : names) {
        std::string path = dir + "/" + name;
        delete_item(dir, name);
    }
}

void batch_copy(const std::string& dir, const std::vector<std::string>& src_names, const std::string& dest_prefix) {
    for (size_t i = 0; i < src_names.size(); ++i) {
        std::string dest_name = dest_prefix + "_" + std::to_string(i + 1);
        copy_file(dir, src_names[i], dest_name);
    }
}

void batch_move(const std::string& dir, const std::vector<std::string>& src_names, const std::string& dest_prefix) {
    for (size_t i = 0; i < src_names.size(); ++i) {
        std::string dest_name = dest_prefix + "_" + std::to_string(i + 1);
        move_file(dir, src_names[i], dest_name);
    }
}