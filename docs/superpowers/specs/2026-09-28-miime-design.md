# MIIME — Linux Image Viewer & Annotation Tool

> A lightweight, fast Linux desktop application for viewing images and annotating them with freehand drawings, shapes, text, and blur effects. Starts instantly, idles under 40 MB RAM, and exports flat PNG files with baked annotations.

## 1. Goals & Constraints

### Primary Goals
- **View images** — open from CLI, browse directories, zoom/pan with low latency
- **Annotate images** — freehand pen, shapes, arrows, text, blur/pixelate, crop
- **Export** — save as flat PNG (annotations baked in), copy to system clipboard

### Hard Constraints
- Cold start < 200 ms to first paint
- Idle RSS < 30 MB (no image loaded), < 40 MB (typical 1080p image)
- Zero CPU at idle (event-driven redraws only, no render loop)
- Annotations are session-only — no sidecar files, no persistent annotation state
- Binary name: `miime`

## 2. Tech Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| **Language** | C99 | Direct memory control, native binary, zero runtime overhead |
| **GUI Toolkit** | GTK4 | Latest toolkit, best Wayland support, actively developed, native Linux integration |
| **2D Rendering** | Cairo | Hardware-agnostic vector rendering, native to GTK, ideal for anti-aliased strokes |
| **Image Decoding** | GdkPixbuf + librsvg | All common raster formats (PNG, JPEG, WebP, BMP, GIF, TIFF) plus SVG |
| **Text Rendering** | Pango/Cairo | System font support, proper glyph shaping |
| **Build System** | Plain Makefile | Minimal overhead, full control, uses `pkg-config` for dependencies |

### Build Dependencies
```
gtk4 (>= 4.0)
cairo
gdk-pixbuf-2.0
librsvg-2.0
pango
pangocairo
```

## 3. Architecture

### 3.1 Layer Model

```
┌─────────────────────────────────────────────┐
│              GtkWindow                      │
│  ┌─────────────────────────────────────┐    │
│  │     MiimeCanvas (GtkDrawingArea)    │    │
│  │  ┌───────────────────────────────┐  │    │
│  │  │  Annotation Layer (Cairo)     │  │    │  ← Vector structs, <1 KB per 100 ops
│  │  ├───────────────────────────────┤  │    │
│  │  │  Base Image (GdkPixbuf → sfc) │  │    │  ← Immutable, single-decode
│  │  └───────────────────────────────┘  │    │
│  └─────────────────────────────────────┘    │
│  ┌──────────────────────┐ ┌──────────────┐  │
│  │ Toolbar (GtkBox)     │ │ Color/Stroke │  │
│  └──────────────────────┘ └──────────────┘  │
└─────────────────────────────────────────────┘
```

### 3.2 Core Principles

- **Non-destructive pipeline:** The original image stays immutable in memory. On save/copy, render the base image into an off-screen Cairo surface, replay vector annotations on top, encode to disk.
- **Vector command stack:** Annotations stored as lightweight structs (`type`, `points`, `color`, `stroke_width`, `opacity`). 100 undo steps ≈ a few KB, not hundreds of MB of bitmap snapshots.
- **Event-driven redraws:** Only `gtk_widget_queue_draw()` on mouse drag, zoom, or tool state change. Idle CPU = 0.0%.
- **Immediate buffer release:** When navigating to next image, free the previous image buffer immediately — no unbounded history.

### 3.3 Data Flow

```
CLI arg / directory scan
        │
        ▼
  image.c: GdkPixbuf load
        │
        ▼
  canvas.c: viewport transform (zoom, pan, fit)
        │
        ├──▶ render.c (on-screen): paint base + replay annotations → GtkDrawingArea
        │
        └──▶ render.c (off-screen): bake base + annotations → Cairo surface
                    │
                    ├──▶ Save: cairo_surface_write_to_png → disk
                    └──▶ Copy: clipboard.c → system clipboard (image/png)
```

## 4. Module Decomposition

### 4.1 File Layout

```
miime/
├── Makefile
├── README.md
├── miime.desktop
├── src/
│   ├── main.c          # Entry point, CLI arg parsing, GtkApplication setup
│   ├── window.c        # MiimeWindow — top-level window, toolbar, layout
│   ├── window.h
│   ├── canvas.c        # MiimeCanvas — drawing area, zoom/pan, draw callback
│   ├── canvas.h
│   ├── image.c         # Image loading (GdkPixbuf), directory scanning
│   ├── image.h
│   ├── annotate.c      # Annotation structs, stack (undo/redo)
│   ├── annotate.h
│   ├── tools.c         # Tool state machine (pen, arrow, rect, text, blur, crop)
│   ├── tools.h
│   ├── render.c        # Cairo rendering: on-screen + off-screen bake + export
│   ├── render.h
│   ├── clipboard.c     # System clipboard copy (image/png)
│   ├── clipboard.h
│   ├── keybinds.c      # Keyboard shortcut registration and dispatch
│   └── keybinds.h
```

### 4.2 Module Dependency Graph

```
main ──▶ window ──▶ canvas ──▶ render ──▶ image
           │          │                     │
           │          │                   (leaf)
           ▼          ▼
         tools ──▶ annotate
                    (leaf)

clipboard ──▶ render
keybinds ──▶ window, canvas, annotate
```

### 4.3 Module Responsibilities

| Module | Responsibility | Dependencies |
|--------|---------------|--------------|
| `main` | CLI parsing (`miime path/to/file.png`), create `GtkApplication`, launch window | `window`, `image` |
| `window` | Top-level layout: toolbar buttons, color swatch, stroke width slider. Routes UI events | `canvas`, `tools` |
| `canvas` | Owns `GtkDrawingArea`. Zoom (scroll-wheel, cursor-anchored), pan (middle/right-click drag), fit modes. Calls `render` in draw callback | `render`, `image`, `annotate` |
| `image` | Loads images via GdkPixbuf, scans directory for next/prev, manages current image lifecycle | — (leaf) |
| `annotate` | Defines `MiimeAnnotation` union/struct, manages annotation stack (dynamic array), push/pop for undo/redo | — (leaf) |
| `tools` | State machine for active tool. Translates mouse events into `MiimeAnnotation` structs pushed onto stack | `annotate` |
| `render` | On-screen: paint base + replay annotations. Off-screen: bake to new surface for export/save | `image`, `annotate` |
| `clipboard` | Copy off-screen rendered surface to system clipboard as `image/png` | `render` |
| `keybinds` | Maps keyboard shortcuts to actions | `window`, `canvas`, `annotate` |

**Design notes:**
- `annotate` and `image` are leaf modules with zero GTK dependencies — pure data, easy to unit test.
- `tools` converts raw input → annotation data (no Cairo). `render` consumes annotations (no input knowledge). Clean separation.
- Target ~200–400 lines per `.c` file. If any grows beyond ~500, split.

## 5. Feature Specification — MVP

### 5.1 Viewer

| Feature | Behavior |
|---------|----------|
| **Open from CLI** | `miime path/to/file.png` — load and display immediately |
| **Directory navigation** | `Space` next image, `Backspace` previous image in same directory (alphabetical sort, async directory scan) |
| **Zoom** | Scroll-wheel zoom anchored to cursor position. Bilinear filtering for downscale, nearest-neighbor at high zoom (pixel inspection) |
| **Pan** | Middle-click or right-click drag |
| **Fit modes** | Fit-to-window (default on open), 1:1 actual pixels (toggle with `1` key) |

### 5.2 Annotation Tools

| Tool | Behavior |
|------|----------|
| **Pen** | Freehand polyline. Configurable color, stroke width (1–20px), opacity (0–100%) |
| **Highlighter** | Same as pen, forced 30% opacity |
| **Line** | Click start → drag → release end. Straight line |
| **Arrow** | Same as line, with triangular arrowhead at end point |
| **Rectangle** | Click corner → drag → release opposite corner. Stroked outline only |
| **Ellipse** | Click bounding-box corner → drag → release. Stroked outline only |
| **Text** | Click to place insertion point, type single line, rendered via Pango/Cairo. `Enter` confirms and commits the text annotation; `Escape` cancels. Configurable font size, color |
| **Blur/Pixelate** | Drag to select rectangular region. On release, apply downscale→upscale pixelation (factor 8×) on a temporary raster patch stored as a special annotation |
| **Crop** | Drag to define bounding box. Box is visible on canvas. Constrains export area only — does not modify canvas view |

### 5.3 Undo/Redo

- `Ctrl+Z` — pop last annotation from stack (undo)
- `Ctrl+Shift+Z` — push back onto stack (redo)
- Redo stack clears when a new annotation is pushed
- Stack has no cap (structs are tiny)

### 5.4 Export

| Action | Trigger | Behavior |
|--------|---------|----------|
| **Save** | `Ctrl+S` | Off-screen bake (base + annotations), write flat PNG. If crop box is set, export only the cropped region. First save prompts for file path via GTK file chooser |
| **Copy** | `Ctrl+C` | Off-screen bake → copy to system clipboard as `image/png` MIME type |

### 5.5 Toolbar UI

- Horizontal `GtkBox` at top of window
- Tool buttons (icon or label): Pen, Highlighter, Line, Arrow, Rect, Ellipse, Text, Blur, Crop
- Color swatch button (opens `GtkColorChooser`)
- Stroke width slider (`GtkScale`, range 1–20)
- Active tool highlighted

### 5.6 Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Q` | Quit |
| `Space` | Next image in directory |
| `Backspace` | Previous image in directory |
| `1` | Toggle fit-to-window / 1:1 pixels |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `Ctrl+S` | Save (bake + export PNG) |
| `Ctrl+C` | Copy composite to clipboard |

### 5.7 Desktop Integration

- `miime.desktop` file with `MimeType=image/png;image/jpeg;image/webp;image/svg+xml;image/bmp;image/gif;image/tiff;`
- `make install` copies binary to `/usr/local/bin/` and desktop file to `/usr/local/share/applications/`

## 6. Explicitly Deferred

| Feature | Reason |
|---------|--------|
| Fit-width / fit-height modes | Fit-to-window covers 90% of needs |
| Thumbnail sidebar / filmstrip | Significant UI + memory complexity |
| Custom arrowhead styles | Single default arrowhead sufficient |
| Filled shapes | Stroked-only for MVP; fill toggle is easy to add later |
| Multi-line text editing | Single-line covers annotation needs |
| JPEG export with quality slider | PNG-only for MVP |
| Stdin pipe (`cat img \| miime -`) | File arg only, simpler |
| Drag-and-drop file open | Nice-to-have, not essential |
| Dark mode / theme switching | GTK4 respects system theme by default |
| Annotation persistence (sidecar files) | Session-only by design |

## 7. Memory & Performance Rules

1. **No tile cache for small images:** For files under 4K resolution, decode to a contiguous RGBA buffer. No complex tiling.
2. **Immediate buffer release:** When navigating to next image, free the previous image buffer. No unbounded navigation history.
3. **No lazy thumbnailing in MVP:** No background thumbnail generation. Deferred to v2.
4. **Annotation stack is structs, not bitmaps:** 100 annotations ≈ a few KB.
5. **Exception — blur patches:** The blur/pixelate tool stores a small raster patch (the pixelated region). This is the only annotation type that allocates pixel data. Size is bounded by the selection rectangle.

## 8. Phased Implementation Roadmap

### Phase 1: Minimal Viewer
*"Open an image, zoom it, browse a directory"*

| Step | Deliverable | Verification |
|------|------------|--------------|
| 1.1 | Makefile + project scaffolding, empty GTK4 window | `make && ./miime` shows blank window |
| 1.2 | `image.c` — load image from CLI arg via GdkPixbuf | Unit: load PNG/JPEG/SVG, verify dimensions |
| 1.3 | `canvas.c` — paint image in GtkDrawingArea, fit-to-window | Visual: image displays correctly |
| 1.4 | Zoom (scroll-wheel, cursor-anchored) + pan (middle-click drag) | Manual test |
| 1.5 | 1:1 fit toggle (`1` key) | Manual test |
| 1.6 | Directory scan + `Space`/`Backspace` navigation | Manual test |
| 1.7 | `Q` to quit | — |
| **Gate** | RSS < 40 MB with 1080p PNG loaded | Fail → fix before Phase 2 |

### Phase 2: Annotation Engine
*"Draw on images, undo, redo"*

| Step | Deliverable | Verification |
|------|------------|--------------|
| 2.1 | `annotate.c` — MiimeAnnotation struct + stack | Unit: push/pop/undo/redo |
| 2.2 | `tools.c` — state machine + pen tool | Manual: freehand drawing |
| 2.3 | `render.c` — replay annotations in draw callback | Visual: annotations persist across redraws |
| 2.4 | Undo/Redo (`Ctrl+Z` / `Ctrl+Shift+Z`) | Manual test |
| 2.5 | Highlighter (30% alpha pen) | Manual test |
| 2.6 | Shapes: line, rectangle, ellipse | Manual test |
| 2.7 | Arrow (line + arrowhead) | Manual test |
| 2.8 | Text tool (Pango rendering) | Manual test |
| 2.9 | Blur/pixelate (box selection + downsample) | Manual test |
| 2.10 | Crop tool (bounding box, export-only) | Manual test |
| 2.11 | Toolbar: tool buttons, color picker, stroke width | Visual: toolbar functional |

### Phase 3: Export & Integration
*"Save, copy, install as a real app"*

| Step | Deliverable | Verification |
|------|------------|--------------|
| 3.1 | Off-screen bake (base + annotations → surface) | Unit: output matches |
| 3.2 | Save (`Ctrl+S`) — flat PNG export | Manual: annotations baked in |
| 3.3 | Crop-aware export | Manual: saved file is cropped |
| 3.4 | Clipboard copy (`Ctrl+C`) | Manual: paste in other app |
| 3.5 | Centralize keybinds | Manual: all shortcuts work |
| 3.6 | `miime.desktop` + `make install` | `desktop-file-validate` passes |
| 3.7 | README.md | Review |
| **Gate** | Full workflow: open → annotate → undo → crop → save → copy → next → quit | All steps work |
