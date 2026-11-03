# Phase 3 — Sketch / Hand-Drawn Renderer

> Goal: implement deterministic, seeded sketch rendering in C/Cairo, mirroring Rough.js algorithms.

## Scope

`sketch.c` produces Cairo paths for each annotation primitive using seeded PRNG. Consumed by `render.c`. Pure math + Cairo, no GTK.

## Deliverables

- `src/sketch.h/.c` (NEW): seeded PRNG + perturbed line/circle/arrow/hachure/variable-width/halo.
- `aesthetic_style` enum wired through canvas_config.
- Visual regression fixtures (PNG golden images).

## Tasks

1. Seeded PRNG: hash annotation id (cyrb128 → Mulberry32); expose `sketch_rand(ctx, min, max)`.
2. Perturbed straight line: endpoint jitter + overshoot, bowing control points, double-stroke.
3. Designer-loop circle/ellipse: angular span with overlap, radial drift harmonics, spiral drift, Catmull-Rom→cubic Bézier conversion.
4. Sketchy arrow: t=0.5 subdivision, asymmetric open V-wings, quadratic Bézier wings.
5. Hachure fill: diagonal scanlines, zig-zag turnaround, perturbed endpoints.
6. Variable-width stroke: pressure profile → filled polygon along normals (Option A); keep Option B (SVG filter) as a future shortcut note only.
7. Sticker halo double-pass in `render.c` (white/#0F172A under-stroke 5.5 px 85% then semantic 2.5 px).
8. `clean_technical` mode: disable perturbation, orthogonal elbows, uniform strokes.
9. Golden-image tests: render fixed annotation set → compare PNG (tolerance-based, e.g. SSIM ≥ 0.99).
10. Verify no redraw jitter: render same annotation 100×, assert identical bytes.

## Verification

- Golden images match within tolerance on Linux x86_64.
- 100 repeated renders produce byte-identical output.
- Pan/zoom of a sketch-annotated image shows no vertex vibration.
- Render of 8 annotations < 100 ms.

## Gate

Determinism verified; visual review approved; no performance regression.

## Dependencies

Phase 0 (schema). Independent of Phases 1 and 2.

## Risks

- Cairo path API differences from SVG; implement conversion carefully (Catmull-Rom → cubic Bézier coefficients).
- Golden-image tests are platform-sensitive (font rendering). Use sketch-only fixtures (no text) for byte-identical checks; text fixtures use SSIM tolerance.
- Variable-width polygon winding order bugs; add unit tests for simple cases.

## Files touched

`src/sketch.h/.c` (new), `src/render.c`, `src/render.h`, `tests/test_sketch.c` (new), `tests/golden/` (new), `Makefile`.
