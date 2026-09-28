CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror $(shell pkg-config --cflags gtk4)
LDFLAGS = $(shell pkg-config --libs gtk4) -lm

SRC = src/main.c src/window.c
OBJ = $(SRC:.c=.o)
BIN = miime

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
	rm -f tests/test_image tests/test_annotate tests/test_render

.PHONY: all clean
