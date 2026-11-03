# 03 — Layout / Automatic Label Placement

## Problem

Automatic label placement is NP-hard in most formulations. Heuristics balance legibility, overlap avoidance, leader-line crossings, and aesthetics. Sources: Wikipedia "Automatic label placement", emergentmind topic (updated 2026-01-18), MERL TR96-04, map-labeling literature.

## Heuristic families (MIIME-relevant)

- **Grid/trellis (Mote)**: subdivide space into quarter-region cells (half a label per dimension); conflict detection only in a constant-size neighborhood (9×9 block); O(n) typical; four candidate positions per feature; greedy cheapest-candidate selection. Sub-second for 10^4–10^5 features. → basis for MIIME's in-canvas 8-position candidate scoring + occupancy grid.
- **Bitmap occupancy**: rasterize obstacles into a 2D bitmap; O(1) overlap checks via bitwise ops; scales to thousands of labels. → MIIME can use a coarse occupancy bitmap for fast collision checks in the force-relaxation stage.
- **External (margin) labeling**: place labels outside the illustration, connect via leaders; greedy/force-directed/metaheuristic; staircase + planarity constraints; capstone/shell pruning for practical runtimes. → MIIME's margin-rail mode is exactly this; topological vertical sort prevents leader crossings.
- **Force-directed / spring**: energy minimization over whole configuration; higher quality but slower. → MIIME uses a bounded 15-iteration spring-repulsion (Hooke's law anchor + MTV repulsion) as a refinement pass, not the primary solver.
- **Deep RL / MARL**: treat each label as an agent; better completeness, high training cost; offline. → out of scope for v2 (no model training).
- **LLM-assisted (MAPLE benchmark)**: LLMs output label positions from cartographic guidelines; 28–33 px RMSE, approaching humans; lacks global conflict resolution; inference latency. → interesting future direction, not v2 core.

## MIIME's 4-stage pipeline (from spec §6)

1. Text pre-measurement (greedy line-break, max 240 px, Pango metrics off-screen).
2. Mode selection: margin rail (≥5 or dense) vs in-canvas 8-position (≤4, ample space).
3. Force relaxation (15 iters, MTV repulsion + spring anchor + boundary clamp).
4. Leader routing (nearest perimeter ports, outward-normal constraint, S-curve or elbow, Liang-Barsky obstacle avoidance).

## Quality criteria encoded

Leader length, label-label overlap, label-target overlap, OOB, un-annotated DOM overlap, visual busyness variance, crossing penalty. Greedy heuristics achieve >90% of optimal coverage in practice; bitmap/grid trade marginal label count for large speed gains.

## Practical tuning guidance

- Real-time/interactive → grid/trellis + bitmap + greedy (MIIME's choice).
- Offline/publication quality → DP/metaheuristic/RL (deferred).
- Context-aware/semantic → LLM-assisted (future).
- Temporal consistency (dynamic maps) → restrict label on/off transitions; out of scope (static screenshots).

## Implications for MIIME 2.0

- Keep the layout engine pure-math and unit-testable (no Cairo).
- Add an occupancy-bitmap fast path for collision checks if profiling shows the O(n²) candidate scoring is slow at 15 annotations.
- Defer RL/LLM placement to a v3 research track.
