# miime Annotation Styles and Image-Safe Themes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status: PROPOSED — not approved, not implemented.**

**Goal:** Add optional drawing styles and image-safe themes while keeping legacy rendering byte-identical when options are omitted.

**Architecture:** Persist requested render config separately from ephemeral resolution. Resolve one deterministic image palette from clean source pixels, apply local contrast checks, then pass a request-scoped resolution through the shared export path used by desktop, headless, and MCP.

**Tech Stack:** C99, GLib, Cairo/Pango, existing C and Python MCP tests; no new runtime dependencies.

**Spec:** `docs/superpowers/specs/2026-11-03-miime-adaptive-themes-design.md`

## Global Constraints

- Omitted options preserve byte-identical 2.1 PNG output; `auto` is opt-in.
- Image analysis samples at most 4096 image points and 64 points per annotation, evenly spaced by the formula in the spec.
- Source alpha composites for analysis over opaque `#FFFFFF` by default; export preserves source alpha/pixels.
- Use the spec's exact token values, palettes, `0.35`/`0.65` luminance thresholds, and contrast formula/targets.
- Requested overrides persist; resolved appearance never serializes; misses remain visible as warnings.
- No UI change, random selection, network, AI, broad source wash, recoloring, or new runtime dependency.

## Review Focus

1. Legacy render path — `test_render_legacy_hashes_without_options` asserts all existing golden PNG hashes unchanged.
2. Transparent input — `test_style_alpha_composite_uses_white_analysis_bg` asserts computed RGB while output alpha remains unchanged.
3. Annotation contamination — `test_style_resolution_uses_clean_source_surface` asserts resolving before/after drawing from the same clean surface yields identical result bytes.
4. Explicit low-contrast override — `test_style_override_is_preserved_and_warned` asserts RGBA is unchanged and warning is set.
5. Render config inheritance/reset — `test_mcp_render_config_inherits_and_resets` asserts omitted patch inherits, explicit reset restores legacy defaults.

---

## File map

| File | Responsibility |
|---|---|
| `src/annotate.h`, `src/schema.c/.h` | Style config, semantic role and override presence fields, request-config serialization |
| `src/style.h/.c` (new) | Bounded sampling, auto selection, contrast, fallback, diagnostics |
| `src/render.h/.c` | Shared options-aware export entry point and style resolution application |
| `src/mcp.c`, `src/headless.c`, `src/main.c` | Parse/persist/pass config for MCP and headless paths |
| MCP tool schema in `src/mcp.c`, `Makefile` | Advertise request config; build style module/tests |
| `tests/test_style.c` (new), `tests/test_schema.c`, `tests/test_render.c`, `tests/test_mcp.py`, `tests/test_headless.c` | Exact acceptance checks |

### Task 1: Requested config schema and persistence

**Files:** `src/annotate.h`, `src/schema.c/.h`; tests `tests/test_schema.c`, `tests/test_bidir.c`.

**Interface:** Define exact enums/`MiimeStyleConfig` from the spec. Add `MiimeSemanticRole role`, `double label_surface_rgba[4]`, and presence flags for `color`, `font_size`, `label_surface`, and role (reuse existing `has_fill_color`) to `MiimeAnnotation`. Implement `char *miime_schema_serialize_render_config(const MiimeStyleConfig *)` and `gboolean miime_schema_deserialize_render_config(const char *, MiimeStyleConfig *, GError **)`. Add `char *miime_schema_serialize_with_config(const MiimeAnnotationStack *, const MiimeStyleConfig *)` for opt-in state/get/patch persistence: emit requested config and omit auto color/font fields when presence is false, even if the old parser populated defaults. No-config calls delegate to the legacy serializer unchanged; synthesized theme colors are never copied into annotation data.

- [ ] **RED:** `test_render_config_round_trip_requested_only` asserts each token survives config serialize/deserialize, resolved metadata is absent, and omitted config produces exact legacy JSON bytes. `test_auto_field_presence_survives_get_patch_reload` asserts omitted color/font remain absent across two opt-in request serializations, while explicit RGBA and font size remain present.
- [ ] **RED:** `test_annotation_override_presence_round_trip` asserts explicit `font_size:0` and explicit RGBA zeroes remain marked present; `test_role_round_trip` covers all four roles.
- [ ] **RED:** `test_render_config_json_validation` asserts canonical hex output, omission rules, and rejection of unknown enum, malformed/alpha hex and wrong-type reset without state mutation.
- [ ] **GREEN:** Add enums, presence flags, config JSON functions and optional role parsing/serialization.
- [ ] Run `make test_schema && make test_bidir`.

### Task 2: Fixed sampler, palettes, and resolver

**Files:** create `src/style.h/.c`; update `Makefile`; create `tests/test_style.c`.

**Interface:** Use exact `MiimeCrop`, `MiimeAnnotationStyleResult`, `MiimeStyleResolution`, and `miime_style_resolve`/`miime_style_resolution_free` definitions from the spec. `miime_style_resolve` receives the clean source surface, requested config, annotation stack, and crop. It returns resolved style/theme and one result per annotation in stack order.

- [ ] **RED:** `test_auto_theme_thresholds_and_ties` asserts below `0.35`→dark, above `0.65`→light, boundary/middle decisions, warm hue `>50%`→warm, otherwise neutral; `test_auto_style_edge_threshold` asserts `>=0.45` technical, lower sketch.
- [ ] **RED:** `test_sampler_caps_and_even_spacing` asserts <=4096 global and <=64 per annotation, deterministic coordinates/counts and actual stroked-path samples rather than empty bounding boxes; `test_style_alpha_composite_uses_white_analysis_bg` checks unpremultiplication and compositing.
- [ ] **GREEN:** Implement specified grid formula, feature calculations, exact hex palette tables, auto tie policy, formula contrast checks, opaque label surface selection, dual white/black halo fallback, coverage and warning codes. Draw no pixels in resolver.
- [ ] Test generated light/dark/warm/terminal/photo/checkerboard surfaces; run `make test_style`.

**Task 2 review follow-up (2026-11-04):** The controller decision ledger governs the `font_size: 0` ambiguity: presence remains explicit and round-trips, but its resolved token uses the legacy 24px fallback. The Luna review's contrary reading of the presence rule is non-blocking; resolver behavior and a regression test now make the accepted precedence explicit. Contrast now composites ink alpha over each sampled source color and evaluates text ink against its resolved label surface (including surface alpha). Coverage counts each passing unique sample, and path samples are deduplicated. The legacy `aesthetic_style` alias now supplies drawing style under `theme: "auto"` when `drawing_style` is absent; explicit `drawing_style` still wins and conflicts remain warned.

**Task 2 review follow-up (2026-11-04, halo coverage):** Non-text samples that miss ink-vs-source contrast are evaluated against both opaque white and black halo edges; either passing edge counts as a hit. `halo_applied` marks use of that fallback, and unresolved/override warnings remain only when neither edge rescues the sample. Text continues to use ink-vs-label-surface contrast without halo fallback. Regression coverage includes a gray-source rescue and a low-opacity override that remains unresolved.

### Task 3: Style-aware shared renderer

**Files:** `src/render.h/.c`; tests `tests/test_render.c`.

**Interface:** Define `MiimeRenderOptions` exactly as spec. Implement `cairo_surface_t *miime_render_export_with_options(MiimeImage *, const MiimeAnnotationStack *, const MiimeRenderOptions *, int *, int *)`. Also implement the exact `miime_render_annotations_with_options(...)` layout/dry-run signature in the spec; it performs one resolution/placement pass before branching into image drawing or dry-run JSON. Both entry points consume the same resolution. Existing export APIs call legacy rendering when options is NULL. Style fields use exact values from the spec; theme colors never mutate source pixels or annotation overrides.

- [ ] **RED:** `test_style_tokens_have_pinned_geometry` asserts technical=2px/4px radius/18px label; sketch=2.5px/8px/18px label; editorial=3px/10px/20px label; minimal=1.5px/2px/16px label (measure path widths/label metrics through Cairo/Pango).
- [ ] **RED:** `test_theme_surface_footprint_and_source_preservation` asserts all five themes leave source RGB/alpha unchanged outside annotation bounds and label surfaces are opaque; `test_local_contrast_fallback_reports_coverage` checks each sampled patch meets target or reports unresolved count.
- [ ] **RED:** `test_margin_rail_crop_uses_source_space_samples` asserts crop-origin and rail-offset mapping; `test_labeled_dry_run_and_render_share_resolution` compares the same resolution object/metadata for labeled exports.
- [ ] **GREEN:** Add options-aware export and layout/dry-run drawing; preserve old export call path and hashes when options are NULL.
- [ ] Run `make test_render` and `test_render_legacy_hashes_without_options`.

### Task 4: MCP/headless request flow and reset semantics

**Files:** `src/mcp.c`, `src/headless.c`, `src/main.c`; MCP input schemas; tests `tests/test_mcp.py`, `tests/test_headless.c`.

**Interface:** Both render and patch accept top-level `drawing_style`, `aesthetic_style`, `theme`, `analysis_background`, and `reset_render_config`. Headless annotation JSON accepts the same config fields. Config precedence: explicit `drawing_style` > legacy alias; absent patch config inherits prior requested config; `reset_render_config:true` clears it before applying fields in that request. MCP and opted-in headless requests call `miime_render_annotations_with_options` with parsed layout mode; basic export uses `miime_render_export_with_options`. Validate `analysis_background` as `#RRGGBB` (canonical uppercase, opaque), enum strings and reset boolean exactly as spec; invalid config must not change state.

- [ ] **RED:** `test_mcp_style_alias_and_precedence` checks old-alias-only requests preserve 2.1 hashes, alias mapping under explicit theme opt-in, and conflict warning; `test_mcp_render_config_inherits_and_resets` checks inheritance/reset; `test_mcp_style_options_are_request_scoped` sends two different consecutive requests and compares selected palettes.
- [ ] **RED:** `test_headless_auto_theme_matches_mcp` renders same fixture/config through both routes and compares output hash and resolved theme.
- [ ] **GREEN:** Parse, inherit/reset, serialize requested config, update tool schemas, and pass request-scoped options from MCP/headless/desktop routes.
- [ ] Run `make test_mcp` and `make test_headless`.

### Task 5: Dry-run diagnostics and integration acceptance

**Files:** `src/render.c/.h`, `src/mcp.c`; tests `tests/test_mcp.py`, `tests/test_style.c`, `tests/test_render.c`.

- [ ] **RED:** `test_dry_run_resolution_matches_render_metadata` asserts same input/crop gives byte-identical resolution fields for dry-run and render: requested/resolved values, background, sampling counts/policy, contrast targets/coverage, per-annotation fallback, and warnings.
- [ ] **RED:** `test_theme_diagnostics_do_not_serialize_resolved_colors` asserts get/patch serialization contains only requests and explicit overrides, never synthesized colors.
- [ ] **GREEN:** Add diagnostics to dry-run and render metadata; keep ordinary render response unchanged and ensure resolution always reads original prepared source/crop.
- [ ] Run `make test`; confirm generated fixture suite passes, all legacy PNG hashes match, and MCP/headless output agrees for the shared fixture.

## Final acceptance

Before implementation is marked complete, all tests above pass; style/theme request config round-trips idempotently; missing patch config inherits; explicit reset restores byte-identical legacy rendering; dry-run/render resolution metadata matches; existing desktop, headless, and MCP callers retain their current defaults.
