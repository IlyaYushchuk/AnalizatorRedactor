#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "state.h"
#include <openssl/md5.h>
#include <ncursesw/ncurses.h>

// Функции анализа
void analysis_init(AnalysisResult *result);
void analysis_free(AnalysisResult *result);
void analysis_perform(AppState *state, time_t old_threshold);
Metadata analysis_get_metadata(const char *full_path);

#endif