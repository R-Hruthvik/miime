# Phase 5 — Bi-Directional Human↔Agent Sync

> Goal: load an agent's JSON layer into the interactive viewer; human edits serialize to a delta the agent can consume.

## Scope

Extend the GUI so an annotation JSON file can be loaded alongside an image; human mouse edits push onto the same `annotations[]` stack and can be exported as a delta JSON.

## Deliverables

- CLI: `miime image.png --annotations layer.json` loads the layer into the viewer.
- Toolbar: "Export annotations" button → writes delta JSON (moved bounds, deleted ids, new annotations).
- Undo/redo works across agent-originated and human-originated annotations uniformly.
- Visual distinction: agent annotations vs human annotations (e.g. subtle badge or color tint) — optional, via `source` field in struct.
- `--annotations-out path.json` opt-in persistence (default off, per v1 constraint).

## Tasks

1. Add `source` enum (human/agent) to `MiimeAnnotation` (backward compatible default human).
2. Load layer JSON on startup when `--annotations` given; validate via `schema.c`.
3. Render loaded annotations through the same `render.c` path (sketch style applied).
4. Export delta: serialize current stack; optionally diff against original to emit only changes.
5. Add toolbar button + keybind (`Ctrl+Shift+E`?) for export; file chooser.
6. Round-trip test: load → move one box → export → reload → verify moved.
7. Document workflow in README (agent → human → agent).

## Verification

- Human moves an agent box, exports delta, agent consumes delta and re-renders correctly.
- Undo/redo across mixed sources works.
- No sidecar file written unless `--annotations-out` given.
- GUI RSS < 40 MB with a 15-annotation layer loaded.

## Gate

Full round-trip demo works; no regression in v1 manual annotation flow.

## Dependencies

Phase 4 (schema + MCP). Uses Phase 2 (layout) and Phase 3 (sketch) in GUI path.

## Risks

- Delta diffing complexity; v2 can export full stack (simpler) and let agent diff — recommend full-stack export for v2, true delta in v3.
- Concurrency between GUI and agent process: file-based handoff only; no shared memory.

## Files touched

`src/window.c`, `src/canvas.c`, `src/annotate.h/.c`, `src/main.c`, `src/keybinds.c`, `README.md`, `tests/test_bidir.c` (new).
