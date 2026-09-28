CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror $(shell pkg-config --cflags gtk4)
LDFLAGS = $(shell pkg-config --libs gtk4) -lm

SRC = src/main.c src/window.c src/image.c src/canvas.c src/render.c src/annotate.c
OBJ = $(SRC:.c=.o)
BIN = miime

TEST_CFLAGS = -std=c99 -Wall -Wextra $(shell pkg-config --cflags gdk-pixbuf-2.0 cairo)
TEST_LDFLAGS = $(shell pkg-config --libs gdk-pixbuf-2.0 cairo)

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
	rm -f tests/test_image tests/test_annotate tests/test_render

test_image: tests/test_image.c src/image.c src/image.h
	$(CC) $(TEST_CFLAGS) tests/test_image.c src/image.c -o tests/test_image $(TEST_LDFLAGS)
	./tests/test_image

test_annotate: tests/test_annotate.c src/annotate.c src/annotate.h
	$(CC) -std=c99 -Wall -Wextra $(shell pkg-config --cflags glib-2.0) tests/test_annotate.c src/annotate.c -o tests/test_annotate $(shell pkg-config --libs glib-2.0)
	./tests/test_annotate

test: test_image test_annotate

.PHONY: all clean test test_image test_annotate

