CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror $(shell pkg-config --cflags gtk4)
LDFLAGS = $(shell pkg-config --libs gtk4) -lm

SRC = src/main.c src/window.c src/image.c src/canvas.c src/render.c src/annotate.c src/tools.c src/clipboard.c
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
	rm -f tests/test_image tests/test_annotate tests/test_render tests/test_tools

test_image: tests/test_image.c src/image.c src/image.h
	$(CC) $(TEST_CFLAGS) tests/test_image.c src/image.c -o tests/test_image $(TEST_LDFLAGS)
	./tests/test_image

test_annotate: tests/test_annotate.c src/annotate.c src/annotate.h
	$(CC) -std=c99 -Wall -Wextra $(shell pkg-config --cflags glib-2.0) tests/test_annotate.c src/annotate.c -o tests/test_annotate $(shell pkg-config --libs glib-2.0)
	./tests/test_annotate

test_render: tests/test_render.c src/render.c src/annotate.c src/image.c
	$(CC) -std=c99 -Wall -Wextra $(shell pkg-config --cflags gtk4) tests/test_render.c src/render.c src/annotate.c src/image.c -o tests/test_render $(shell pkg-config --libs gtk4) -lm
	./tests/test_render

test_tools: tests/test_tools.c src/tools.c src/annotate.c src/image.c
	$(CC) -std=c99 -Wall -Wextra $(shell pkg-config --cflags gtk4) tests/test_tools.c src/tools.c src/annotate.c src/image.c -o tests/test_tools $(shell pkg-config --libs gtk4) -lm
	./tests/test_tools

test: test_image test_annotate test_render test_tools

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 data/com.miime.app.desktop $(DESTDIR)$(DATADIR)/applications/
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps
	install -m 644 data/icons/com.miime.app.svg $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -f $(DESTDIR)$(DATADIR)/applications/com.miime.app.desktop
	rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/com.miime.app.svg

.PHONY: all clean test test_image test_annotate test_render install uninstall

