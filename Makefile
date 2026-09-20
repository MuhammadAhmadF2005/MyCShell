CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

TARGET = mysh
PREFIX = CS311_A01_2034335_
OBJS = $(PREFIX)main.o $(PREFIX)shell.o $(PREFIX)parser.o $(PREFIX)builtins.o $(PREFIX)process.o $(PREFIX)linenoise.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(PREFIX)main.o: $(PREFIX)main.c $(PREFIX)shell.h $(PREFIX)parser.h $(PREFIX)builtins.h $(PREFIX)process.h $(PREFIX)linenoise.h
	$(CC) $(CFLAGS) -c $(PREFIX)main.c -o $@

$(PREFIX)shell.o: $(PREFIX)shell.c $(PREFIX)shell.h $(PREFIX)parser.h $(PREFIX)builtins.h $(PREFIX)process.h
	$(CC) $(CFLAGS) -c $(PREFIX)shell.c -o $@

$(PREFIX)parser.o: $(PREFIX)parser.c $(PREFIX)parser.h
	$(CC) $(CFLAGS) -c $(PREFIX)parser.c -o $@

$(PREFIX)builtins.o: $(PREFIX)builtins.c $(PREFIX)builtins.h $(PREFIX)parser.h $(PREFIX)process.h
	$(CC) $(CFLAGS) -c $(PREFIX)builtins.c -o $@

$(PREFIX)process.o: $(PREFIX)process.c $(PREFIX)process.h
	$(CC) $(CFLAGS) -c $(PREFIX)process.c -o $@

$(PREFIX)linenoise.o: $(PREFIX)linenoise.c $(PREFIX)linenoise.h
	$(CC) $(CFLAGS) -c $(PREFIX)linenoise.c -o $@

clean:
	rm -f $(TARGET) $(OBJS)
