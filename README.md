# MIIME

MIIME is a fast, lightweight Linux image viewer, annotation tool, and (v2) agent-harness visual feedback engine. Built in C with GTK4/Cairo.

![MIIME Icon](data/icons/com.miime.app.svg)

## Features (v2)

- **Blazingly fast image viewing**: immediate rendering, instant navigation, fit-to-window scaling.
- **Non-destructive vector annotations**: draw freely; annotations remain in vector space until flattened on export.
- **Rich toolset**: Pen, Highlighter, Line, Rectangle, Ellipse, Arrow, Text, Blur/Pixelate, Crop.
- **Set-of-Mark grounding**: pre-tag UI elements with numbered badges on a reference image; agents address elements by ID instead of guessing pixel coordinates.
- **Declarative annotation batches**: three high-level tools (`prepare_ux_canvas`, `render_ux_annotations`, `patch_ux_annotations`) drive a full review in one turn.
- **Collision-free layout engine**: margin-rail / in-canvas placement with force relaxation and leader routing; no overlapping callouts.
- **Sketch aesthetic**: deterministic seeded hand-drawn rendering (no redraw jitter) with sticker halos; `rough_sketch` (default) or `clean_technical`.
- **Self-verification loop**: render → agent inspects → patch → re-render before the user sees anything.
- **Bi-directional sync**: load an agent's JSON layer into the interactive viewer; human edits serialize back to a JSON delta.
- **Headless / MCP mode**: drive MIIME without a visible window; produce annotated PNG/WebP buffers from the CLI or via an MCP server.
- **Export**: Save (`Ctrl+S`), Save As (`Ctrl+Shift+S`), Clipboard (`Ctrl+C`).
- **Low memory & Wayland-first**: event-driven redraws, zero idle CPU, minimal RSS.

## Keybindings & Shortcuts

| Action | Shortcut / Key |
|---|---|
| Zoom in / out | Scroll wheel (centered at cursor) |
| Pan viewport | Middle-click drag / Right-click drag |
| Toggle Fit / 1:1 | `1` |
| Next image in folder | `Space` |
| Previous image in folder | `Backspace` |
| Toggle Toolbar | `Tab` |
| Undo annotation | `Ctrl + Z` |
| Redo annotation | `Ctrl + Shift + Z` |
| Save image | `Ctrl + S` |
| Save image as... | `Ctrl + Shift + S` |
| Copy to clipboard | `Ctrl + C` |
| Quit application | `Q` |
| Select Pen tool | `P` |
| Select Highlighter tool | `H` |
| Select Line tool | `L` |
| Select Rectangle tool | `R` |
| Select Ellipse tool | `E` |
| Select Arrow tool | `A` |
| Select Text tool | `T` (Enter to commit, Esc to cancel) |
| Select Blur tool | `B` |
| Select Crop tool | `C` |

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

## Building & Installing

### Compile

```bash
make
```

### Build the MCP server (no GTK dependency)

```bash
make miime-mcp
```

### Run Tests

```bash
make test
```

### Install System-wide

```bash
sudo make install
```

Installs the `miime` binary to `/usr/local/bin`, the `.desktop` launcher, and the application icon.

### Uninstall

```bash
sudo make uninstall
```

## Usage

```bash
# Open an image
miime photo.png

# Open an empty window
miime

# Load an agent annotation layer and export edits on quit
miime photo.png --annotations review.json --annotations-out edited.json

# Headless render (no window)
miime --headless in.png [--annotations a.json] -o out.png
```

## MCP Setup

MIIME ships a separate stdio MCP binary, `miime-mcp`, with no GTK dependency so it runs on servers and CI.

Exposed tools: `prepare_ux_canvas`, `render_ux_annotations`, `patch_ux_annotations`, `get_ux_annotations`, and `verify_ux_render`. Their MCP `inputSchema` values describe the supported arguments and annotation vocabulary.

`render_ux_annotations` and `patch_ux_annotations` accept `layout_mode` (`auto`, `margin_rail`, or `in_canvas_floating`), `aesthetic_style` (`rough_sketch` or `clean_technical`), and an `annotations` array. Annotation types include `highlight_box`, `spacing_ruler`, `alignment_redline`, `callout`, `pin`, `freehand`, `circle`, `bracket`, `underline`, `strike`, and the legacy `pen`, `line`, `rect`, `ellipse`, `arrow`, `text`, `blur`, and `crop` types. Styling fields include `ink` (`thin`, `regular`, `bold`, `marker`), `stroke_width` (a positive number or a two-number taper), `dash`, `fill`, `fill_color`, `radius`, `arrow`, and `z`. Points may be a flat coordinate list or a list of coordinate pairs; `som_id` targets an element identified during preparation. Legacy `text`, `font_size`, and `source` fields remain accepted. A first-class `label` can set `text`, `anchor`, and `leader` (`curved`, `elbow`, or `none`). Set `dry_run: true` to receive label placements without a PNG.

```json
{
  "layout_mode": "margin_rail",
  "aesthetic_style": "clean_technical",
  "dry_run": true,
  "annotations": [{
    "type": "freehand",
    "points": [[20, 30], [42, 48], [68, 35]],
    "ink": "bold",
    "stroke_width": [4, 2],
    "label": {"text": "Review this flow", "anchor": "right", "leader": "elbow"}
  }]
}
```

### Claude Code

```bash
claude mcp add miime /usr/local/bin/miime-mcp
```

### Generic MCP client (`.mcp.json`)

```json
{
  "mcpServers": {
    "miime": {
      "command": "/usr/local/bin/miime-mcp",
      "args": []
    }
  }
}
```

## License

MIT
