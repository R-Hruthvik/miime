# miime Annotation Styles and Image-Safe Themes

**Status: APPROVED — implemented on 2026-11-04.**

## Goal and compatibility

Add optional drawing styles and palette themes while preserving byte-identical 2.1 output when both are omitted. `auto` is explicit opt-in. A legacy request containing only `aesthetic_style` stays on the exact 2.1 path; aliases select the new style tokens only when `drawing_style` or `theme` explicitly activates the new resolver. Keep requested values separate from ephemeral resolved appearance: serialization stores only user requests and overrides; patch requests with omitted config inherit the prior requested config, while `{"reset_render_config":true}` restores legacy defaults. Resolution is recomputed from the clean source on every render.

## Fixed vocabulary

Drawing style controls geometry; theme controls semantic-neutral colors and label surfaces. A present `drawing_style` wins over `aesthetic_style`; legacy `rough_sketch` maps to `sketch`, `clean_technical` maps to `technical`. If both are present and disagree, use `drawing_style` and report `style_alias_conflict`.

All drawing colors below are sRGB hex. `stroke_width` is px. All labels use Pango family `DejaVu Sans`; font sizes are rendered pixels, set with Pango absolute sizing. Default label sizes are at least 16px so full-size screenshot notes remain readable. Style defaults apply only when no corresponding user override is present.

| Style | Stroke | Text / title | Corner radius | Halo | Fill alpha |
|---|---:|---:|---:|---|---:|
| `technical` | 2 px, round cap | 18 / 22 px, regular/semibold | 4 px | 1 px | 0.12 |
| `sketch` | 2.5 px, existing seeded rough geometry | 18 / 22 px, regular/semibold | 8 px, rough path | 1.5 px | 0.18 |
| `editorial` | 3 px, round cap | 20 / 26 px, regular/bold | 10 px | 2 px | 0.16 |
| `minimal` | 1.5 px, round cap | 16 / 20 px, regular/semibold | 2 px | 1 px | 0.08 |

Use fills only for explicit fill annotations and label surfaces. Never recolor source pixels. `editorial` uses the same family with weight and size hierarchy (no new font dependency). The `sketch` style reuses existing deterministic rough rendering.

| Theme | Ink | Label surface | Secondary surface | Status colors: error / warning / info |
|---|---|---|---|---|
| `light` | `#202124` | `#FFFFFF` | `#F1F3F4` | `#B3261E` / `#8A4B00` / `#1558A6` |
| `dark` | `#F1F3F4` | `#202124` | `#303134` | `#FF6B6B` / `#FFC857` / `#72B7FF` |
| `warm` | `#352A24` | `#FFF8ED` | `#F1E4D1` | `#A52A2A` / `#825000` / `#245B8F` |
| `neutral` | `#222222` | `#FFFFFF` | `#E8E8E8` | `#A61B1B` / `#785500` / `#185A8D` |
| `auto` | resolved to one of the four palettes above | same | same | same semantic colors as selected palette |

Role (`none|error|warning|info`) is independent of theme hue and per-annotation. Role-driven colors require an active style/theme; without one, role is persisted metadata and legacy appearance remains unchanged. Explicit annotation color, fill color, font size, or label surface overrides the corresponding resolved default; add presence bits for `color`, `fill_color`, `font_size`, and label surface so explicit zero/default values remain intentional. An override is preserved even when it misses contrast; emit a warning.

## Deterministic image analysis and contrast

`MiimeStyleConfig` contains `drawing_style`, `theme`, opaque `analysis_background_rgba` (default `#FFFFFF`), `has_drawing_style`, `has_theme`, and `has_analysis_background`. For source alpha, compute analysis RGB as `src_rgb * alpha + background_rgb * (1-alpha)` in sRGB; unpremultiply Cairo ARGB32 channels before this calculation. Analysis background affects diagnostics only: export preserves source alpha and pixels exactly; no implicit flattening.

Sample at most **4096** evenly spaced deterministic grid points over the clean source/crop for image-level features. At most **64** evenly spaced deterministic points per annotation bounds for local contrast checks. Use `nx=min(width,64)`, `ny=min(height,64)` for global sampling; local flat regions use `nx=min(width,8)`, `ny=min(height,8)`. Stroke checks distribute up to 64 samples along the actual flattened stroked path, not empty bounding-box interiors; label text is checked against its opaque surface. Use integer index mapping `floor((i + 0.5) * extent / count)` independently in x/y, deduplicate repeated coordinates, and report actual count. No sample may come from an annotated render. Compute mean linear-light relative luminance, coarse hue bin (8 bins), and edge density (fraction of neighboring samples with luminance delta >= 0.15).

For `auto`, choose `dark` if mean luminance < 0.35; choose `light` if > 0.65; otherwise choose `warm` if warm hue bins (red/orange/yellow) exceed 50%; otherwise choose `neutral`. On exact threshold equality use the middle branch (`0.35` and `0.65` both enter the middle range). Edge density is diagnostic and selects drawing style only when requested style is omitted *and theme is `auto`*: density >= 0.45 selects `technical`; otherwise `sketch`. For explicit theme with omitted style, retain legacy style (`sketch`). This rule is stable per clean image/crop.

Contrast uses this relative-luminance engineering formula: linearize sRGB channel `c` with `c/12.92` when `c <= 0.04045`, else `((c+0.055)/1.055)^2.4`; luminance `L=0.2126R+0.7152G+0.0722B`; ratio `(max(L1,L2)+0.05)/(min(L1,L2)+0.05)`. Targets are engineering targets, not a claim of standards certification: **4.5:1** for text/label text and **3:1** for shapes, measured after alpha compositing on each sampled source color. For labels choose the theme's opaque label surface and ink pair with the higher minimum sampled text ratio. For strokes that miss target, add a dual neutral halo: opaque white outer stroke and opaque black inner stroke, each at the style's `halo` width; measure foreground against the adjacent halo edge. If any local sample still misses target, report `contrast_unresolved` with hit/total coverage; do not claim universal photo safety.

## Request JSON contract

The document root accepts `drawing_style` and `theme` as the exact string enums above, `analysis_background` as an opaque `#RRGGBB` string, and `reset_render_config` as a boolean. Hex digits are case-insensitive; serialization emits uppercase `#RRGGBB`. Reject unknown enums, malformed hex, alpha-bearing strings, wrong types, or non-boolean reset with a schema error before changing persisted state. Requested-config serialization emits only explicitly requested fields; derived style/theme/background values are never inserted. `reset_render_config` is a command, not stored state. Annotation `role` uses the four role strings; `label_surface` is a four-number RGBA array with finite channels in `[0,1]`. Request-level `layout_mode` retains its existing enum and meaning.

## Interfaces and diagnostics

Proposed C API (exact public types/signatures for implementation):

```c
typedef enum { MIIME_DRAW_TECHNICAL, MIIME_DRAW_SKETCH,
               MIIME_DRAW_EDITORIAL, MIIME_DRAW_MINIMAL } MiimeDrawingStyle;
typedef enum { MIIME_THEME_AUTO, MIIME_THEME_LIGHT, MIIME_THEME_DARK,
               MIIME_THEME_WARM, MIIME_THEME_NEUTRAL } MiimeTheme;
typedef enum { MIIME_ROLE_NONE, MIIME_ROLE_ERROR,
               MIIME_ROLE_WARNING, MIIME_ROLE_INFO } MiimeSemanticRole;
typedef struct {
    MiimeDrawingStyle drawing_style;
    MiimeTheme theme;
    guint32 analysis_background_rgba; /* opaque 0xRRGGBBAA; default 0xFFFFFFFF */
    gboolean has_drawing_style, has_theme, has_analysis_background;
} MiimeStyleConfig;
typedef struct { int x, y, width, height; gboolean enabled; } MiimeCrop;
typedef struct {
    char annotation_id[32];
    guint local_sample_count, contrast_hits;
    gboolean halo_applied, contrast_unresolved, override_warning;
} MiimeAnnotationStyleResult;
typedef struct MiimeStyleResolution {
    MiimeStyleConfig requested;
    MiimeDrawingStyle resolved_drawing_style;
    MiimeTheme resolved_theme;
    guint image_sample_count, contrast_hits, contrast_total;
    double text_target, shape_target;
    char policy[64], warning_codes[256];
    MiimeAnnotationStyleResult *annotations;
    guint annotation_count;
} MiimeStyleResolution;
typedef struct {
    MiimeStyleConfig requested;
    const MiimeStyleResolution *resolution; /* borrowed, optional */
} MiimeRenderOptions;
MiimeStyleResolution *miime_style_resolve(const cairo_surface_t *clean_source,
    const MiimeStyleConfig *requested, const MiimeAnnotationStack *stack,
    MiimeCrop crop);
void miime_style_resolution_free(MiimeStyleResolution *resolution);
cairo_surface_t *miime_render_export_with_options(MiimeImage *img,
    const MiimeAnnotationStack *stack, const MiimeRenderOptions *options,
    int *out_w, int *out_h);
char *miime_render_annotations_with_options(MiimeImage *img,
    const MiimeAnnotationStack *stack, int layout_mode,
    MiimeAestheticStyle legacy_aesthetic, const MiimeRenderOptions *options,
    gboolean dry_run, cairo_surface_t **out_surface, int *out_w, int *out_h);
char *miime_schema_serialize_render_config(const MiimeStyleConfig *config);
char *miime_schema_serialize_with_config(const MiimeAnnotationStack *stack,
    const MiimeStyleConfig *config);
gboolean miime_schema_deserialize_render_config(const char *json,
    MiimeStyleConfig *out, GError **error);
```

The options-aware layout entry point above owns both drawing and dry-run: resolve styles and place labels once before branching into drawing or JSON assembly. Both MCP and opted-in headless requests call it with the parsed `layout_mode`; `miime_render_export_with_options` is a convenience wrapper using auto layout for new options. Existing export/layout APIs retain their 2.1 compatibility paths. Crop positions and all contrast samples use clean source-image coordinates; label/rail output coordinates are mapped back through crop origin and the 300px rail offset only where source pixels exist. Rail label surfaces are tested against their declared opaque surface, not clamped to edge pixels of the source image. Diagnostics record the coordinate space and effective crop/offset. Never sample an already composited output to compute resolution.

`MiimeStyleResolution` contains requested/resolved style and theme, policy/background, image and local sample counts, contrast targets and hit/total coverage, per-annotation fallback/warning codes. Add render config and `reset_render_config` to both MCP input schemas and headless JSON config. Dry-run and render metadata for identical input/crop must match byte-for-byte; ordinary render response remains unchanged. `miime_schema_serialize_with_config` emits requested top-level config and annotation overrides only. Under opt-in styles/themes it omits `color`/`font_size` when their presence flags are false, including parser-generated legacy defaults; it must not turn an automatically styled field into an explicit override after get/patch/reload. With no opt-in config it delegates to the unchanged legacy serializer. Missing theme and drawing style remain missing (never serialize a resolved choice).

## Scope and tradeoffs

This design changes how supplied annotations are rendered. It does not identify screenshot content, decide what deserves annotation, or generate annotation text; those remain caller-authored inputs. The sample image used for a live preview is separate from implementation acceptance fixtures.

Generated light, dark, warm, terminal-like, photo-like and checkerboard fixtures only; no user screenshots or private checked-in fixtures. No UI changes, source wash, recoloring, random choice, network, AI, or runtime dependencies. Palette-only selection misses local readability; arbitrary per-stroke hues flicker and clash; global mutable options leak across concurrent requests; these alternatives are excluded in favor of one deterministic image palette plus local contrast protection.
