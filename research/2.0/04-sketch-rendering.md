# 04 — Sketch / Hand-Drawn Rendering

## Rough.js (roughjs.com, MIT, ~9 kB gzipped)

Lightweight JS library for sketchy hand-drawn graphics on Canvas and SVG. Primitives: lines, curves, arcs, polygons, circles, ellipses, SVG paths. Fill styles: hachure (default), solid, zigzag, cross-hatch, dots, sunburst, dashed, zigzag-line. Configurable `roughness`, `bowing`, `stroke`, `strokeWidth`, `fillWeight`, `hachureAngle`, `hachureGap`.

Core algorithms adapted from the "handy" processing library (for line/ellipse outlines) and Mozilla (SVG arc→canvas). This is the reference implementation MIIME's C sketch module should mirror conceptually.

## Biomechanical effects to simulate (from spec §7)

1. **Ballistic overshoot**: lines cross slightly past vertices.
2. **Mid-stroke bowing**: long straight lines bow outward (wrist pivot).
3. **Multi-stroke overlap**: circles don't close cleanly; tail overlaps start by 15–30°.
4. **Velocity-based tapering**: strokes taper at start/end, thin in fast mid-section.

## Critical engineering constraint: deterministic seeding

Never use `Math.random()` in the render loop — causes "jittering redraw bug" on pan/zoom/label updates. Hash the annotation id (cyrb128/Murmur3) → Mulberry32 PRNG; every perturbation calls `rand()` from that seed. Result: unique organic shape per annotation, frozen across redraws. (MIIME sketch module must be stateless: recompute from id hash each render.)

## Rendering techniques

- **Perturbed straight line**: endpoint jitter + overshoot along normal; cubic Bézier control points at 1/3 and 2/3 with shared signed bowing term (clamped ~8 px) + high-frequency tremor; double-stroke (full-opacity 2.5 px + 45%-opacity 1.2 px ghost).
- **Designer-loop circle/ellipse**: angular span 2π + 0.25–0.55 rad overlap; low-frequency radial drift (sin/cos harmonics + linear spiral drift ~4 px); Catmull-Rom → cubic Bézier. Avoids jagged star artifacts from white noise.
- **Sketchy arrow**: subdivide leader Bézier at t=0.5, offset midpoint ±3 px; asymmetric open V-wings with independent angles (28°±5°) and lengths (14±2 px); each wing a quadratic Bézier bowed 1.5 px.
- **Hachure fill**: diagonal scanlines at −41°, 8 px spacing, zig-zag turnaround, endpoints perturbed 2–6 px inward, 15% opacity.
- **Variable-width stroke**: pressure profile `w(t)=w_base·(1−0.25 sin πt)·min(1,8t)·min(1,8(1−t))` → taper first/last 12%; build filled polygon along normals. Alternative: SVG `feTurbulence` + `feDisplacementMap` for lightweight roughening.
- **Sticker halo**: every path rendered twice — white (or #0F172A on pure white) 5.5 px under-stroke at 85% opacity, then semantic color 2.5 px over-stroke. Guarantees legibility on any background.

## Implications for MIIME 2.0

- Implement `sketch.c` as a pure-C module mirroring Rough.js's algorithms (line/ellipse perturbation, hachure, bowing) but deterministic and seeded by annotation id.
- Cairo path construction maps directly: cubic Bézier, quadratic Bézier, filled polygons.
- Halo double-pass is cheap in Cairo and essential for sketch-on-photo legibility.
- Keep `roughness`/`bowing` as tunable constants behind the `aesthetic_style` enum (`rough_sketch` vs `clean_technical`).
- No JS dependency in the binary; Rough.js is a reference, not a runtime.
