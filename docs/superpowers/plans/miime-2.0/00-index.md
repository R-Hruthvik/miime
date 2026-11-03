# MIIME 2.0 — Implementation Plan Index

> Spec: `docs/superpowers/specs/2026-11-02-miime-2.0-design.md`
> Research: `research/2.0/`

This directory breaks the MIIME 2.0 roadmap (spec §13) into one file per phase.
Each phase file contains: goal, scope, deliverables, tasks, verification, gate, dependencies, and risk notes.

## Phases

| # | File | Title | Depends on |
|---|------|-------|------------|
| 0 | `phase-0-foundation.md` | Refactor: state-driven annotations + schema + headless | v1 shipped |
| 1 | `phase-1-grounding.md` | SoM preprocessing (DOM/CV/ML) | Phase 0 |
| 2 | `phase-2-layout.md` | Collision-free layout engine | Phase 0 |
| 3 | `phase-3-sketch.md` | Hand-drawn sketch renderer | Phase 0 |
| 4 | `phase-4-tool-surface.md` | Tool schema + MCP binary + self-verification | Phases 1–3 |
| 5 | `phase-5-bidirectional.md` | Bi-directional human↔agent sync | Phase 4 |
| 6 | `phase-6-polish.md` | Polish, docs, demo, release | Phase 5 |

## Execution order

Phases 1, 2, 3 are independent after Phase 0 and can run in parallel.
Phase 4 integrates them. Phases 5 and 6 are sequential.

## Global constraints (from spec §1, §12)

- Cold start < 200 ms; idle RSS < 40 MB; zero CPU at idle.
- ~200–400 lines per `.c` file; split beyond ~500.
- Leaf modules (`annotate`, `image`, `layout`, `sketch`) have zero GTK deps → unit-testable.
- No sidecar files by default; opt-in `--annotations-out`.
- Annotations session-only unless harness opt-in.

## Global gates

Every phase must pass: `make && make test` green, no RSS regression, no new compiler warnings.
