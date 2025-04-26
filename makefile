CC = g++
CFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = -lncursesw
TARGET = cursach
OBJECTS = main.o module_analization.o module_redactor.o UI.o state_manager.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

main.o: main.cpp state_manager.h UI.h
	$(CC) $(CFLAGS) -c main.cpp -o main.o

module_analization.o: module_analization.cpp module_analization.h
	$(CC) $(CFLAGS) -c module_analization.cpp -o module_analization.o

module_redactor.o: module_redactor.cpp module_redactor.h
	$(CC) $(CFLAGS) -c module_redactor.cpp -o module_redactor.o

UI.o: UI.cpp UI.h state_manager.h module_redactor.h
	$(CC) $(CFLAGS) -c UI.cpp -o UI.o

state_manager.o: state_manager.cpp state_manager.h module_analization.h module_redactor.h UI.h
	$(CC) $(CFLAGS) -c state_manager.cpp -o state_manager.o

clean:
	rm -f $(TARGET) $(OBJECTS)

rebuild: clean all