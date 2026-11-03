CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror $(shell pkg-config --cflags gtk4)
LDFLAGS = $(shell pkg-config --libs gtk4) -lm

SRC = src/main.c src/window.c src/image.c src/canvas.c src/render.c src/layout.c src/annotate.c src/tools.c src/clipboard.c src/schema.c src/headless.c src/sketch.c src/freehand.c
OBJ = $(SRC:.c=.o)
BIN = miime

TEST_CFLAGS = -std=c99 -Wall -Wextra -Werror -Isrc $(shell pkg-config --cflags gtk4 glib-2.0 gdk-pixbuf-2.0 cairo pango pangocairo)
TEST_LDFLAGS = $(shell pkg-config --libs gtk4 glib-2.0 gdk-pixbuf-2.0 cairo pango pangocairo) -lm

all: $(BIN) miime-mcp

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN) miime-mcp
	rm -f tests/test_image tests/test_annotate tests/test_render tests/test_tools tests/test_schema tests/test_headless tests/test_sketch tests/test_grounding tests/test_layout tests/test_bidir tests/test_mcp

test_sketch: tests/test_sketch.c src/sketch.c src/freehand.c src/render.c src/layout.c src/annotate.c src/image.c
	$(CC) $(TEST_CFLAGS) tests/test_sketch.c src/sketch.c src/freehand.c src/render.c src/layout.c src/annotate.c src/image.c -o tests/test_sketch $(TEST_LDFLAGS)
	./tests/test_sketch

test_image: tests/test_image.c src/image.c
	$(CC) $(TEST_CFLAGS) tests/test_image.c src/image.c -o tests/test_image $(TEST_LDFLAGS)
	./tests/test_image

test_annotate: tests/test_annotate.c src/annotate.c
	$(CC) $(TEST_CFLAGS) tests/test_annotate.c src/annotate.c -o tests/test_annotate $(TEST_LDFLAGS)
	./tests/test_annotate

test_render: tests/test_render.c src/render.c src/layout.c src/sketch.c src/freehand.c src/annotate.c src/image.c
	$(CC) $(TEST_CFLAGS) tests/test_render.c src/render.c src/layout.c src/sketch.c src/freehand.c src/annotate.c src/image.c -o tests/test_render $(TEST_LDFLAGS)
	./tests/test_render

test_tools: tests/test_tools.c src/tools.c src/annotate.c src/image.c
	$(CC) $(TEST_CFLAGS) tests/test_tools.c src/tools.c src/annotate.c src/image.c -o tests/test_tools $(TEST_LDFLAGS)
	./tests/test_tools

test_grounding: tests/test_grounding.c src/grounding.c
	$(CC) $(TEST_CFLAGS) tests/test_grounding.c src/grounding.c -o tests/test_grounding $(TEST_LDFLAGS)
	./tests/test_grounding

test_schema: tests/test_schema.c src/schema.c src/annotate.c
	$(CC) $(TEST_CFLAGS) tests/test_schema.c src/schema.c src/annotate.c -o tests/test_schema $(TEST_LDFLAGS)
	./tests/test_schema

test_layout: tests/test_layout.c src/layout.c
	$(CC) -std=c99 -Wall -Wextra -Werror tests/test_layout.c src/layout.c -o tests/test_layout -lm
	./tests/test_layout

test_headless: tests/test_headless.c src/headless.c src/schema.c src/render.c src/layout.c src/sketch.c src/freehand.c src/annotate.c src/image.c
	$(CC) $(TEST_CFLAGS) tests/test_headless.c src/headless.c src/schema.c src/render.c src/layout.c src/sketch.c src/freehand.c src/annotate.c src/image.c -o tests/test_headless $(TEST_LDFLAGS)
	./tests/test_headless

test_bidir: tests/test_bidir.c src/schema.c src/annotate.c
	$(CC) $(TEST_CFLAGS) tests/test_bidir.c src/schema.c src/annotate.c -o tests/test_bidir $(TEST_LDFLAGS)
	./tests/test_bidir

MCP_CFLAGS = -D_GNU_SOURCE -std=c99 -Wall -Wextra -Werror -Isrc $(shell pkg-config --cflags glib-2.0 gdk-pixbuf-2.0 cairo pango pangocairo)
MCP_LDFLAGS = $(shell pkg-config --libs glib-2.0 gdk-pixbuf-2.0 cairo pango pangocairo) -lm

miime-mcp: src/mcp_main.c src/mcp.c src/grounding.c src/headless.c src/schema.c src/render.c src/layout.c src/sketch.c src/freehand.c src/annotate.c src/image.c
	$(CC) $(MCP_CFLAGS) src/mcp_main.c src/mcp.c src/grounding.c src/headless.c src/schema.c src/render.c src/layout.c src/sketch.c src/freehand.c src/annotate.c src/image.c -o miime-mcp $(MCP_LDFLAGS)

test_mcp: miime-mcp
	python3 tests/test_mcp.py

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share

install: $(BIN) miime-mcp
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -m 755 miime-mcp $(DESTDIR)$(BINDIR)/miime-mcp
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 data/com.miime.app.desktop $(DESTDIR)$(DATADIR)/applications/
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps
	install -m 644 data/icons/com.miime.app.svg $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN) $(DESTDIR)$(BINDIR)/miime-mcp
	rm -f $(DESTDIR)$(DATADIR)/applications/com.miime.app.desktop
	rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/com.miime.app.svg

test: test_schema test_layout test_bidir test_image test_annotate test_render test_tools test_sketch test_headless test_grounding test_mcp
	@echo "ALL TESTS PASSED"

.PHONY: all clean test test_image test_annotate test_render test_tools test_schema test_headless test_grounding test_layout test_bidir test_mcp miime-mcp
