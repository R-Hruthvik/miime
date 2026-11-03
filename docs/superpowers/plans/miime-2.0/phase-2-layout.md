# Phase 2 — Collision-Free Layout Engine

> Goal: implement the 4-stage automatic label placement pipeline as a pure-math, unit-testable module.

## Scope

`layout.c` consumes `MiimeAnnotation[]` + image dimensions + registry; outputs final label positions, leader ports, and curve control points. No Cairo, no GTK.

## Deliverables

- `src/layout.h/.c` (NEW): full pipeline (text measurement, mode selection, force relaxation, leader routing).
- Occupancy-bitmap fast path for collision checks (optional, profile-gated).
- Unit test suite with golden fixtures (overlap, overflow, crossing prevention).

## Tasks

1. Text pre-measurement: greedy line-break at max 240 px using Pango font metrics (passed in as a callback or measured in a thin wrapper); compute label W/H + header height.
2. Mode selection logic: count annotations + density heuristic → margin_rail vs in_canvas_floating vs auto.
3. Margin-rail: bipartite partition by target center; topological vertical sort; 1D sweep with min-gap; reverse sweep on overflow.
4. In-canvas: 8 candidate positions per target at 24 px offset; scoring function with weights (target overlap 1000, OOB 1000, label overlap 500, DOM overlap 50, busyness 10); pick lowest.
5. Force relaxation: 15 iterations; MTV repulsion (12 px safety buffer); Hooke's-law spring anchor toward initial candidate; boundary clamp.
6. Leader routing: nearest perimeter port selection with outward-normal dot-product constraint; S-curve (cubic Bézier, 0.4×span control projection) for rough_sketch; orthogonal elbow for clean_technical.
7. Obstacle avoidance: Liang-Barsky line-rect intersection; padded-corner detour as quadratic Bézier midpoint.
8. Z-index assignment (painter's algorithm order).
9. Occupancy bitmap fast path (profile with `MIIME_LAYOUT_BITMAP=1`).
10. Unit tests: `tests/test_layout.c` with ≥ 20 cases (single, pair, group, overflow, crossing, OOB).

## Verification

- 8 annotations on a dense 1440×900 dashboard render with zero overlap warnings.
- Leader arrows never cross on margin-rail test fixtures.
- Layout of 15 annotations completes < 50 ms.
- All unit tests green; golden-file diffs stable.

## Gate

Zero overlap on test fixtures; deterministic output (same input → identical positions); no regression in v1 render.

## Dependencies

Phase 0 (schema). Independent of Phase 1 and 3.

## Risks

- Font metrics in a GTK-free module: pass a measurement callback or use a minimal Pango context. Keep `layout` Cairo/GTK-free by accepting `measure_text_fn`.
- Golden-file brittleness: use tolerance-based comparison for floating point.
- Force relaxation non-convergence on pathological inputs: cap iterations, fall back to last valid state.

## Files touched

`src/layout.h/.c` (new), `tests/test_layout.c` (new), `Makefile`.
