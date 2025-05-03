#!/bin/bash

# Проверяем, передан ли путь к папке
if [ -z "$1" ]; then
    echo "Использование: $0 <папка> [выходной_файл]"
    echo "Пример: $0 ./src output.txt"
    exit 1
fi

# Папка для поиска
SEARCH_DIR="$1"

# Выходной файл (по умолчанию 'output.txt')
OUTPUT_FILE="${2:-output.txt}"

# Очищаем выходной файл (если существует) или создаём новый
> "$OUTPUT_FILE"

# Ищем все .h и .cpp файлы и обрабатываем их
find "$SEARCH_DIR" -type f \( -name "*.h" -o -name "*.cpp" \) | while read -r file; do
    # Получаем только имя файла (без пути)
    filename=$(basename "$file")
    echo "**\`$filename\`:**" >> "$OUTPUT_FILE"
    echo "\`\`\`" >> "$OUTPUT_FILE"
    cat "$file" >> "$OUTPUT_FILE"
    echo -e "\n\`\`\`\n" >> "$OUTPUT_FILE"
done

echo "Готово! Результат сохранён в $OUTPUT_FILE"