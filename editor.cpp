#include "editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm> // Для std::min

void editor_init(EditorState *editor) {
    if (!editor) {
        fprintf(stderr, "Error: editor_init called with NULL editor\n");
        return;
    }
    editor->lines = NULL;
    editor->line_count = 0;
    editor->capacity = 0;
    editor->cursor_x = 0;
    editor->cursor_y = 0;
    editor->scroll_y = 0;
    // printf("Editor initialized\n");
}

void editor_free(EditorState *editor) {
    if (!editor) {
        fprintf(stderr, "Error: editor_free called with NULL editor\n");
        return;
    }
    for (size_t i = 0; i < editor->line_count; ++i) {
        free(editor->lines[i]);
        editor->lines[i] = NULL;
    }
    free(editor->lines);
    editor->lines = NULL;
    editor->line_count = 0;
    editor->capacity = 0;
    // printf("Editor freed\n");
}

void editor_load_file(EditorState *editor, const char *filename) {
    if (!editor) {
        fprintf(stderr, "Error: editor_load_file called with NULL editor\n");
        return;
    }
    if (!filename) {
        fprintf(stderr, "Error: editor_load_file called with NULL filename\n");
        return;
    }
    // printf("Loading file: %s\n", filename);
    editor_free(editor);

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        editor->lines = (char **)malloc(sizeof(char *));
        if (!editor->lines) {
            fprintf(stderr, "Failed to allocate memory for lines\n");
            return;
        }
        editor->lines[0] = strdup("");
        if (!editor->lines[0]) {
            fprintf(stderr, "Failed to duplicate empty line\n");
            free(editor->lines);
            editor->lines = NULL;
            return;
        }
        editor->line_count = 1;
        editor->capacity = 1;
        // printf("Created empty file with 1 line\n");
        return;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    editor->capacity = 10;
    editor->lines = (char **)malloc(editor->capacity * sizeof(char *));
    if (!editor->lines) {
        fprintf(stderr, "Failed to allocate memory for lines\n");
        fclose(file);
        return;
    }

    while ((read = getline(&line, &len, file)) != -1) {
        if (editor->line_count >= editor->capacity) {
            editor->capacity *= 2;
            char **new_lines = (char **)realloc(editor->lines, editor->capacity * sizeof(char *));
            if (!new_lines) {
                fprintf(stderr, "Failed to reallocate memory for lines\n");
                free(line);
                fclose(file);
                editor_free(editor);
                return;
            }
            editor->lines = new_lines;
        }
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        editor->lines[editor->line_count] = strdup(line);
        if (!editor->lines[editor->line_count]) {
            fprintf(stderr, "Failed to duplicate line\n");
            free(line);
            fclose(file);
            editor_free(editor);
            return;
        }
        editor->line_count++;
    }

    free(line);
    fclose(file);

    if (editor->line_count == 0) {
        editor->lines = (char **)malloc(sizeof(char *));
        if (!editor->lines) {
            fprintf(stderr, "Failed to allocate memory for empty lines\n");
            return;
        }
        editor->lines[0] = strdup("");
        if (!editor->lines[0]) {
            fprintf(stderr, "Failed to duplicate empty line\n");
            free(editor->lines);
            editor->lines = NULL;
            return;
        }
        editor->line_count = 1;
        editor->capacity = 1;
    }
    // printf("File loaded: %zu lines\n", editor->line_count);
}

static void editor_save_file(EditorState *editor, const char *filename) {
    if (!editor || !filename) {
        fprintf(stderr, "Error: editor_save_file called with NULL editor or filename\n");
        return;
    }
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Failed to save file: %s\n", filename);
        return;
    }

    for (size_t i = 0; i < editor->line_count; ++i) {
        if (editor->lines[i]) {
            fprintf(file, "%s\n", editor->lines[i]);
        } else {
            fprintf(file, "\n");
        }
    }

    fclose(file);
}