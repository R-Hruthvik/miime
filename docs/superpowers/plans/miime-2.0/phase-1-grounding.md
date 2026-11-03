# Phase 1 — SoM Grounding

> Goal: produce a Set-of-Mark reference image + compact Grounding Registry from a URL or static image.

## Scope

Implement `grounding.c` with three tracks: DOM extraction (live URL), coordinate grid (always), CV/ML fallback (static images). Output Buffer B + registry. External ML backend only (HTTP), never in-process.

## Deliverables

- `src/grounding.h/.c` (NEW): `miime_grounding_run(source_type, source, viewport, visual_aids)` → writes Buffer B PNG path + registry JSON.
- DOM extractor (invoked via headless browser / Playwright subprocess): pruning + telemetry.
- Coordinate grid renderer (0–1000, major/minor lines, in-canvas watermarks, adaptive contrast).
- CV contour fallback (Canny + dilation) for static images.
- Optional OmniParser HTTP client (configurable URL, cached by image hash).
- SoM badge placement with collision sliding + category colors.
- `prepare_ux_canvas` logic wired into headless (Phase 4 exposes as tool).

## Tasks

1. Define `GroundingRegistry` struct (array of elements: id, tag, bounds, telemetry, contrast).
2. Implement coordinate grid renderer on a Cairo surface (Buffer B base).
3. Implement CV contour fallback (use stb or simple Canny; no heavy deps; target < 200 ms for 1080p).
4. Implement DOM extractor wrapper: spawn Playwright script (shipped in `tools/extract_dom.js`) that prints JSON to stdout; parse in C.
5. Implement pruning heuristics (visibility, occlusion, semantic priority, IoU dedup).
6. Implement telemetry extraction + WCAG contrast pre-computation.
7. Implement SoM badge placement with occupancy grid collision avoidance.
8. Implement OmniParser HTTP client (libcurl) + image-hash cache (file under ~/.cache/miime/).
9. Unit tests: pruning logic, grid watermark coordinates, badge non-overlap.

## Verification

- `prepare_ux_canvas` on a live URL returns SoM image + registry with 50–120 elements.
- Static PNG path (CV fallback) returns ≥ 20 elements on a typical UI screenshot.
- Runtime < 2 s for typical page (DOM track); < 500 ms for CV fallback.
- RSS < 40 MB (no in-process model).

## Gate

SoM image visually readable; registry compact (< 50 KB for 100 elements); no overlap between badges on test fixtures.

## Dependencies

Phase 0 (schema + headless).

## Risks

- Playwright dependency heavy; make it optional (configurable path). If absent, fall back to CV-only and warn.
- DOM extraction subprocess latency; cache by URL+viewport.
- CV contour quality on low-contrast UIs; document limitations.

## Files touched

`src/grounding.h/.c` (new), `tools/extract_dom.js` (new), `src/image.c` (hash helper), `Makefile`, `tests/test_grounding.c` (new).
