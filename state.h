#ifndef STATE_H
#define STATE_H

#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// Режимы работы программы
typedef enum {
    MODE_BROWSE,    // Просмотр файлов/папок
    MODE_ANALYSIS,  // Режим анализа
    MODE_EDITOR,     // Режим редактора
    MODE_SEARCH      // Режим поиска
} AppMode;

// Структура для хранения информации о файле/папке
typedef struct {
    char *name;         // Имя файла/папки (может быть относительный путь при поиске)
    int is_dir;         // 1 = папка, 0 = файл
    off_t size;         // Размер в байтах
    time_t mtime;       // Время последнего изменения
} FileInfo;

// Структура состояния редактора
typedef struct {
    char **lines;       // Массив строк
    long long line_count;  // Количество строк
    long long capacity;    // Вместимость массива строк
    unsigned int cursor_x;  // Позиция курсора по X
    unsigned int cursor_y;  // Позиция курсора по Y
    unsigned int scroll_y;  // Смещение прокрутки
} EditorState;

// Структура для хранения информации о дубликатах
typedef struct {
    std::string hash;          // MD5-хеш файла
    std::vector<std::string> paths; // Список путей к файлам с одинаковым хешем
} DuplicateInfo;

// Структура для хранения результатов анализа
typedef struct {
    std::vector<std::string> old_files;      // Давно не используемые файлы
    std::vector<std::string> empty_files;    // Пустые файлы
    std::vector<std::string> empty_dirs;     // Пустые директории
    std::vector<DuplicateInfo> duplicates;   // Дубликаты
    long long selected_index;                    // Индекс выбранного элемента
    enum { SECTION_OLD, SECTION_EMPTY_FILES, SECTION_EMPTY_DIRS, SECTION_DUPLICATES } section; // Текущий раздел
} AnalysisResult;

// Структура состояния приложения
typedef struct {
    AppMode mode;               // Текущий режим
    char *current_dir;          // Текущая директория
    std::vector<FileInfo> files; // Список файлов/папок
    long long selected_index;   // Индекс выбранного файла/папки
    char *edit_file;            // Имя файла, открытого в редакторе
    EditorState *editor_state;  // Состояние редактора
    AnalysisResult *analysis_result; // Результаты анализа
    char *search_query;         // Поисковый запрос
    std::vector<FileInfo> filtered_files; // Отфильтрованные файлы
    char *clipboard_path;       // Путь к файлу/папке в буфере обмена
    bool clipboard_is_cut;      // Флаг: true для вырезания, false для копирования
} AppState;

// Функции менеджера состояния
void state_init(AppState *state);
void state_free(AppState *state);
void state_set_current_dir(AppState *state, const char *path);
void state_load_files(AppState *state);
void state_select_index(AppState *state, unsigned int index);
void state_set_edit_file(AppState *state, const char *filename);
void state_filter_files(AppState *state, const char *query);
void state_collect_files_recursive(const char *base_path, const char *relative_path, const char *query, std::vector<FileInfo> &files);

#endif