#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "state.h"
#include <string>
#include <vector>
#include <openssl/md5.h>

// Структура для хранения информации о дубликатах
typedef struct {
    char *hash;              // MD5-хеш файла
    std::vector<char *> paths; // Список путей к файлам с одинаковым хешем
} DuplicateInfo;

// Структура для хранения результатов анализа
typedef struct {
    std::vector<char *> old_files;      // Давно не используемые файлы
    std::vector<char *> empty_files;    // Пустые файлы
    std::vector<DuplicateInfo> duplicates; // Дубликаты
} AnalysisResult;

// Функции анализа
void analysis_init(AnalysisResult *result);
void analysis_free(AnalysisResult *result);
void analysis_perform(AppState *state, time_t old_threshold);
void analysis_draw(AppState *state);
int analysis_handle_input(AppState *state);

#endif