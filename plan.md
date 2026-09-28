To build a Linux image viewer and annotation tool that starts instantly and idles under 30–40 MB of RAM, avoid web runtimes (Electron/Tauri) and heavy application frameworks. The standard approach for low-overhead Linux desktop utilities is **C or Rust paired with GTK3/GTK4 and Cairo**, similar to tools like *Swappy* and *Viewnior*.

---

### 1. Recommended Tech Stack

| Component | Choice | Rationale |
| --- | --- | --- |
| **Language** | **C** or **Rust** | Direct memory control, native binary, zero runtime or garbage collection overhead. |
| **GUI Toolkit** | **GTK3** (or GTK4) | Native Wayland and X11 support, built-in Linux desktop integration (file choosers, clipboard, dark mode). |
| **2D Vector Engine** | **Cairo** | Hardware-agnostic 2D vector rendering. Native to GTK, ideal for anti-aliased pens, arrows, text, and shapes. |
| **Image Decoding** | **GdkPixbuf** or **libspng / stb_image** | Fast loading for common formats (PNG, JPEG, WebP, SVG) without bringing in heavyweight multimedia libraries. |

*(Alternative for zero-toolkit builds: C99 + SDL2 + NanoVG. This drops GTK entirely for raw windowing, but requires writing custom file dialogs and window decorations).*

---

### 2. Core Architectural Design

To keep memory consumption low while supporting multi-step undo/redo, decouple the raw bitmap from the user's edits.

```
┌────────────────────────────────────────────────────────┐
│                      Window / Canvas                   │
├────────────────────────────────────────────────────────┤
│  [ Overlay Layer ]  Vector drawing commands (Cairo)    │  <-- Tiny memory footprint (structs)
├────────────────────────────────────────────────────────┤
│  [ Base Image ]     Loaded bitmap (GdkPixbuf / RGB)    │  <-- Read-only in memory
└────────────────────────────────────────────────────────┘
                           │
                           ▼ On "Save" / "Copy"
               Bake Base + Overlay to Export Buffer

```

* **Vector Command Stack (Undo/Redo):** Store annotations as a list of lightweight structs (e.g., `type: ARROW, points: [x1, y1, x2, y2], color: 0xFF0000, stroke_width: 3`) rather than caching full-frame raster bitmaps. Storing 100 undo steps uses a few kilobytes of RAM instead of hundreds of megabytes.
* **Non-Destructive Pipeline:** The original image stays immutable in memory. On save or clipboard export, render the base image into an off-screen Cairo surface, replay the vector commands on top, and encode directly to disk.
* **Event-Driven Redraws:** Never run an unbounded rendering loop. Only request a frame redraw (`gtk_widget_queue_draw`) on mouse drag, zoom change, or tool state change. Idle CPU usage drops to 0.0%.

---

### 3. Feature Breakdown

#### Viewer Core

* **Navigation:** Next/Previous image in current directory (read directory asynchronously; preload only the immediate adjacent filenames, not full bitmaps).
* **Viewport Transforms:** Smooth zoom anchored to cursor position (bilinear filtering for downscaling, nearest-neighbor for high zoom levels to inspect pixels), panning via middle-click or right-click drag.
* **Fit Modes:** Fit to window, 1:1 actual pixels, fit width/height.

#### Annotation & Marking Tools

* **Pen / Highlighter:** Freehand lines with configurable opacity (alpha channel in Cairo).
* **Shapes:** Lines, arrows (with customizable heads), rectangles, and ellipses.
* **Redaction (Blur / Pixelate):** Apply a fast box-blur or downscale-upscale pixelation shader to a selected bounding box directly on a temporary layer.
* **Text Tool:** Basic text overlay using Pango/Cairo with system fonts.
* **Crop Tool:** Bounding-box selection that re-bounds the canvas export area without modifying original disk files until saved.

#### Linux Integration

* **CLI Invocation:** Accept files via arguments (`viewer path/to/file.png`) and standard input (`cat file.png | viewer -`).
* **Clipboard:** Direct copy to Wayland/X11 clipboard (`image/png` MIME type).
* **Desktop File:** Standard `.desktop` entry with support for common image MIME types.

---

### 4. Memory and Performance Rules

1. **Avoid Tile-Cache Overhead for Small Images:** For files under 4K resolution, decode straight to a contiguous RGB/RGBA buffer. Avoid complex tiling engines like GEGL unless supporting massive gigapixel images.
2. **Lazy Thumbnailing:** If a directory thumbnail bar is added later, generate thumbnails on a separate background thread with low priority, capped at a maximum cache size (e.g., 50 MB LRU cache).
3. **Release Unused Buffers:** When moving to the next image, free the previous image buffer immediately rather than retaining an unbounded navigation history.

---

### 5. Implementation Roadmap & Verification Checklist

#### Phase 1: Minimal Canvas Viewer

* [ ] Initialize a GTK3/GTK4 window with a `GtkDrawingArea`.
* [ ] Load a local image via `GdkPixbuf` and paint it in the drawing area using `gdk_cairo_set_source_pixbuf`.
* [ ] Implement mouse scroll-wheel zoom anchored to the cursor coordinate.
* [ ] Verify baseline memory usage (`ps_mem` or `htop` under 25 MB RSS).

#### Phase 2: Vector Annotation Engine

* [ ] Define the core `Annotation` struct and a dynamic array / vector list to hold them.
* [ ] Hook `button-press-event`, `motion-notify-event`, and `button-release-event` to record line/rect points.
* [ ] In the Cairo draw callback, iterate the annotation array and stroke each element over the image.
* [ ] Add `Ctrl+Z` (pop last element from array) and verify instant undo.

#### Phase 3: Export & Utilities

* [ ] Implement an off-screen render function that writes the combined canvas to PNG using `cairo_surface_write_to_png`.
* [ ] Add crop box coordinates to constrain the final export rectangle.
* [ ] Bind keyboard shortcuts: `Q` (quit), `Space` / `Backspace` (next/prev image), `Ctrl+C` (copy composite to clipboard).
