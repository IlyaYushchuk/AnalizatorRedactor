CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = -lncursesw -lstdc++fs  # Добавлено для компоновки
TARGET = cursach
SRCS = main.cpp module_analization.cpp

# Цель по умолчанию
all: build

# Компиляция программы
build: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)  # Добавлено $(LDFLAGS)
	@echo "Программа успешно скомпилирована."

# Запуск программы
run: $(TARGET)
	@echo "Запуск программы..."
	./$(TARGET)

# Очистка, перекомпиляция и запуск
rebuild_run: clean build run

# Очистка скомпилированных файлов
clean:
	rm -f $(TARGET)
	@echo "Скомпилированные файлы удалены."

# Флаг для предотвращения конфликтов с файлами
.PHONY: all build run rebuild_run clean