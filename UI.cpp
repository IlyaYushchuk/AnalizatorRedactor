#include <ncursesw/ncurses.h> // Используем ncursesw для поддержки русского текста
#include <locale.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FILES 10
#define MAX_PATH 256

typedef struct {
    wchar_t name[MAX_PATH]; // Используем wchar_t для поддержки UTF-8
    long size;
    char type[10];
    char last_used[50];
} FileInfo;

typedef enum {
    MENU_MAIN,
    MENU_FILES,
    MENU_ANALYSIS,
    MENU_EDIT,
    MENU_STATS,
    MENU_LOGS
} MenuState;

// Инициализация ncurses с поддержкой широких символов
void init_ncurses() {
    setlocale(LC_ALL, ""); // Включаем поддержку локалей для UTF-8
    initscr();
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK); // Цвет для текста
    init_pair(2, COLOR_WHITE, COLOR_BLUE); // Цвет для выделения
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
}

// Главное меню
void draw_main_menu(int highlight) {
    const wchar_t *choices[] = {L"Анализ", L"Редактирование", L"Статистика", L"Выход"};
    int n_choices = sizeof(choices) / sizeof(wchar_t *);
    
    clear();
    mvprintw(0, 0, "Главное меню");
    for(int i = 0; i < n_choices; i++) {
        if(highlight == i) {
            attron(COLOR_PAIR(2));
            mvprintw(i + 2, 0, "> %ls", choices[i]); // %ls для wchar_t
            attroff(COLOR_PAIR(2));
        } else {
            mvprintw(i + 2, 0, "  %ls", choices[i]);
        }
    }
    refresh();
}

// Заглушка для списка файлов
void get_dummy_file_list(FileInfo *files, int *count) {
    *count = 5; // Фиксированное количество файлов для заглушки
    wcscpy(files[0].name, L"Файл1.txt"); files[0].size = 1024; strcpy(files[0].type, "file"); strcpy(files[0].last_used, "2025-03-20");
    wcscpy(files[1].name, L"Папка_тест"); files[1].size = 0; strcpy(files[1].type, "dir"); strcpy(files[1].last_used, "2025-03-15");
    wcscpy(files[2].name, L"Документ.doc"); files[2].size = 2048; strcpy(files[2].type, "file"); strcpy(files[2].last_used, "2025-03-10");
    wcscpy(files[3].name, L"Фото.jpg"); files[3].size = 5120; strcpy(files[3].type, "file"); strcpy(files[3].last_used, "2025-03-01");
    wcscpy(files[4].name, L"Архив.zip"); files[4].size = 3072; strcpy(files[4].type, "file"); strcpy(files[4].last_used, "2025-02-25");
}

// Отображение списка файлов
void draw_file_list(FileInfo *files, int count, int highlight) {
    clear();
    mvprintw(0, 0, "Список файлов (Имя | Размер | Тип | Последнее использование)");
    for(int i = 0; i < count; i++) {
        if(highlight == i) {
            attron(COLOR_PAIR(2));
            mvprintw(i + 2, 0, "> %20ls | %10ld | %-5s | %s", 
                files[i].name, files[i].size, files[i].type, files[i].last_used);
            attroff(COLOR_PAIR(2));
        } else {
            mvprintw(i + 2, 0, "  %20ls | %10ld | %-5s | %s", 
                files[i].name, files[i].size, files[i].type, files[i].last_used);
        }
    }
    refresh();
}

// Панель анализа (заглушка)
void draw_analysis_panel(FileInfo *files, int count) {
    clear();
    mvprintw(0, 0, "Панель анализа");
    mvprintw(2, 0, "Старые файлы (>30 дней):");
    mvprintw(3, 0, "- %ls (Последнее использование: %s)", files[4].name, files[4].last_used);
    mvprintw(5, 0, "Дубликаты: (не найдено)");
    mvprintw(6, 0, "Пустые директории: (не найдено)");
    refresh();
}

// Панель редактирования (заглушка)
void draw_edit_panel(FileInfo *files, int count, int highlight) {
    clear();
    mvprintw(0, 0, "Панель редактирования");
    mvprintw(2, 0, "Выбранный файл: %ls", files[highlight].name);
    mvprintw(4, 0, "Доступные действия:");
    mvprintw(5, 0, "- Удалить");
    mvprintw(6, 0, "- Переместить");
    mvprintw(7, 0, "- Переименовать");
    mvprintw(9, 0, "Нажмите Enter для действия, Esc для выхода");
    refresh();
}

// Панель статистики (заглушка)
void draw_stats_panel() {
    clear();
    mvprintw(0, 0, "Статистика файловой системы");
    mvprintw(2, 0, "Общий размер файлов: 11.2 КБ");
    mvprintw(3, 0, "Количество файлов: 4");
    mvprintw(4, 0, "Количество директорий: 1");
    mvprintw(6, 0, "График использования:");
    mvprintw(7, 0, "[#####     ] 50%% занято");
    refresh();
}

// Панель логов (заглушка)
void draw_logs_panel() {
    clear();
    mvprintw(0, 0, "Логи действий");
    mvprintw(2, 0, "2025-03-22 10:00: Удалён файл 'Старый_файл.txt'");
    mvprintw(3, 0, "2025-03-22 10:01: Перемещена папка 'Архив'");
    refresh();
}

int main() {
    init_ncurses();
    
    MenuState state = MENU_MAIN;
    int highlight = 0;
    int ch;
    FileInfo files[MAX_FILES];
    int file_count = 0;
    
    while(1) {
        switch(state) {
            case MENU_MAIN:
                draw_main_menu(highlight);
                ch = getch();
                switch(ch) {
                    case KEY_UP:
                        if(highlight > 0) highlight--;
                        break;
                    case KEY_DOWN:
                        if(highlight < 3) highlight++;
                        break;
                    case 10: // Enter
                        if(highlight == 0) state = MENU_ANALYSIS;
                        else if(highlight == 1) state = MENU_EDIT;
                        else if(highlight == 2) state = MENU_STATS;
                        else if(highlight == 3) goto exit;
                        highlight = 0;
                        break;
                    case 27: // Esc
                        goto exit;
                }
                break;
                
            case MENU_FILES:
                get_dummy_file_list(files, &file_count);
                draw_file_list(files, file_count, highlight);
                ch = getch();
                if(ch == KEY_UP && highlight > 0) highlight--;
                else if(ch == KEY_DOWN && highlight < file_count - 1) highlight++;
                else if(ch == 27) state = MENU_MAIN;
                break;
                
            case MENU_ANALYSIS:
                get_dummy_file_list(files, &file_count);
                draw_analysis_panel(files, file_count);
                ch = getch();
                if(ch == 27) state = MENU_MAIN;
                break;
                
            case MENU_EDIT:
                get_dummy_file_list(files, &file_count);
                draw_edit_panel(files, file_count, highlight);
                ch = getch();
                if(ch == KEY_UP && highlight > 0) highlight--;
                else if(ch == KEY_DOWN && highlight < file_count - 1) highlight++;
                else if(ch == 27) state = MENU_MAIN;
                break;
                
            case MENU_STATS:
                draw_stats_panel();
                ch = getch();
                if(ch == 27) state = MENU_MAIN;
                break;
                
            case MENU_LOGS:
                draw_logs_panel();
                ch = getch();
                if(ch == 27) state = MENU_MAIN;
                break;
        }
    }
    
exit:
    endwin();
    return 0;
}