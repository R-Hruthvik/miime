# MIIME

A blazingly fast, lightweight image viewer and annotation tool for Linux built with C and GTK4.

![MIIME Icon](data/icons/com.miime.app.svg)

## Features

- **Blazingly fast image viewing**: Immediate rendering, instant navigation, and fit-to-window scaling.
- **Directory navigation**: Seamlessly cycle through all images in a folder using `Space` and `Backspace`.
- **Non-destructive vector annotations**: Draw freely over images with zero degradation. Annotations remain in vector space until flattened on export.
- **Rich toolset**:
  - **Pen**: Freehand sketching
  - **Highlighter**: 30% alpha highlighting
  - **Line**: Straight line segment
  - **Rectangle**: Outlined box
  - **Ellipse**: Outlined oval
  - **Arrow**: Pointed vector arrow
  - **Text**: Crisp Pango font text placement
  - **Blur / Pixelate**: 8× box-downsampled privacy obscuring
  - **Crop**: Non-destructive cropping region for exports
- **Exporting**:
  - **Save (`Ctrl+S`)** / **Save As (`Ctrl+Shift+S`)**: Bakes base image, annotations, and crop into a flat PNG/JPEG.
  - **Clipboard (`Ctrl+C`)**: Bakes and copies texture to the system clipboard directly for pasting into Discord, Slack, browsers, or image editors.
- **Low memory & Wayland-first**: Event-driven architecture with zero idle CPU wakeups and minimal memory footprint.

---

## Keybindings & Shortcuts

| Action | Shortcut / Key |
|---|---|
| **Zoom in / out** | Scroll wheel (centered at cursor) |
| **Pan viewport** | Middle-click drag / Right-click drag |
| **Toggle Fit / 1:1** | `1` |
| **Next image in folder** | `Space` |
| **Previous image in folder** | `Backspace` |
| **Toggle Toolbar** | `Tab` |
| **Undo annotation** | `Ctrl + Z` |
| **Redo annotation** | `Ctrl + Shift + Z` |
| **Save image** | `Ctrl + S` |
| **Save image as...** | `Ctrl + Shift + S` |
| **Copy to clipboard** | `Ctrl + C` |
| **Quit application** | `Q` |
| **Select Pen tool** | `P` |
| **Select Highlighter tool** | `H` |
| **Select Line tool** | `L` |
| **Select Rectangle tool** | `R` |
| **Select Ellipse tool** | `E` |
| **Select Arrow tool** | `A` |
| **Select Text tool** | `T` (Enter to commit, Esc to cancel) |
| **Select Blur tool** | `B` |
| **Select Crop tool** | `C` |

---

## Dependencies

- `gcc` (C99 support)
- `gtk4` (>= 4.10)
- `cairo`
- `gdk-pixbuf-2.0`
- `pango` & `pangocairo`
- `librsvg-2.0`

### Debian / Ubuntu
```bash
sudo apt-get install -y libgtk-4-dev libcairo2-dev libgdk-pixbuf-2.0-dev librsvg2-dev libpango1.0-dev
```

---

## Building & Installing

### Compile
```bash
make
```

### Run Tests
```bash
make test
```

### Install System-wide
```bash
sudo make install
```
This installs the `miime` binary to `/usr/local/bin`, installs the `.desktop` launcher to `/usr/local/share/applications`, and installs the application icon.

### Uninstall
```bash
sudo make uninstall
```

---

## Usage

```bash
# Open an image
miime photo.png

# Open an empty window
miime
```

---

## License

MIT
