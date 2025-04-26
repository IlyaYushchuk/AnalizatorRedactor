#ifndef STATE_H
#define STATE_H

#include <stdlib.h>
#include <string.h>
#include "analysis.h"

// Режимы работы программы
typedef enum {
    MODE_BROWSE,    // Просмотр файлов/папок
    MODE_ANALYSIS,  // Режим анализа
    MODE_EDITOR     // Режим редактора
} AppMode;

// Структура для хранения информации о файле/папке
typedef struct {
    char *name;         // Имя файла/папки
    int is_dir;         // 1 = папка, 0 = файл
    off_t size;         // Размер в байтах
    time_t mtime;       // Время последнего изменения
} FileInfo;

// Структура состояния редактора
typedef struct {
    char **lines;       // Массив строк
    size_t line_count;  // Количество строк
    size_t capacity;    // Вместимость массива строк
    unsigned int cursor_x;  // Позиция курсора по X
    unsigned int cursor_y;  // Позиция курсора по Y
    unsigned int scroll_y;  // Смещение прокрутки
} EditorState;

// Структура состояния приложения
typedef struct {
    AppMode mode;               // Текущий режим
    char *current_dir;          // Текущая директория
    FileInfo *files;            // Список файлов/папок
    size_t file_count;          // Количество файлов/папок
    unsigned int selected_index;  // Индекс выбранного файла/папки
    char *edit_file;            // Имя файла, открытого в редакторе
    EditorState *editor_state;  // Состояние редактора
    AnalysisResult *analysis_result; // Результаты анализа
} AppState;

// Функции менеджера состояния
void state_init(AppState *state);
void state_free(AppState *state);
void state_set_current_dir(AppState *state, const char *path);
void state_load_files(AppState *state);
void state_select_index(AppState *state, unsigned int index);
void state_set_edit_file(AppState *state, const char *filename);

#endif