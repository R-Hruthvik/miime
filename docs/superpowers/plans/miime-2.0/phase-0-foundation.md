# Phase 0 — Foundation Refactor

> Goal: decouple annotation state from mouse events; add JSON schema + headless pipeline. No new user-visible features.

## Scope

Refactor v1 so that `annotations[]` is the single source of truth, readable/writable as JSON, and renderable without a window. This unblocks all later phases.

## Deliverables

- `src/annotate.c/.h` extended: stable `MiimeAnnotation` struct with a `json_id` field; push/pop/clear API unchanged.
- `src/schema.c/.h` (NEW): JSON↔struct serialization/validation for the v2 schema (canvas_config + annotations + target addressing modes).
- `src/headless.c/.h` (NEW): windowless Cairo surface pipeline: load image → apply JSON → render → write PNG/WebP.
- `src/tools.c` refactored: mouse tools become *producers* pushing onto `annotations[]`; no direct Cairo.
- CLI: `miime --headless in.png --annotations a.json -o out.png`.
- Unit tests: `tests/test_schema.c`, `tests/test_headless.c`.

## Tasks

1. Add `json_id` (char[32]) to `MiimeAnnotation`; keep ABI-compatible where possible.
2. Implement `schema_serialize()` / `schema_deserialize()` using a lightweight JSON parser (cJSON or hand-rolled; prefer cjson if available, else vendored single-file).
3. Validate required fields, enums, addressing modes, maxItems 15, maxLength constraints.
4. Implement headless pipeline reusing `render.c` off-screen bake; no GTK init.
5. Wire CLI flags in `main.c` (detect `--headless`).
6. Update `Makefile` to compile new modules; add test targets.
7. Run `make test`; ensure v1 tests still green.

## Verification

- `make && make test` green.
- `miime --headless tests/fixtures/in.png --annotations tests/fixtures/a.json -o /tmp/out.png` produces a valid PNG with baked annotations.
- RSS of headless process < 40 MB for 1080p input.
- Cold start of GUI unchanged (< 200 ms).

## Gate

Existing v1 tests pass; headless round-trip works; no RSS regression.

## Dependencies

v1 shipped (current repo state).

## Risks

- JSON parser choice: avoid heavy deps. If cJSON unavailable, vendor a single-file parser (~2k LOC) — budget ~1 day.
- `annotate.c` struct changes ripple into `tools.c` and `render.c`; keep diff minimal (minimal-change discipline).
- Headless must not link GTK; verify `ldd` on `miime` headless binary.

## Files touched

`src/annotate.h`, `src/annotate.c`, `src/schema.h`, `src/schema.c` (new), `src/headless.h`, `src/headless.c` (new), `src/main.c`, `src/tools.c`, `Makefile`, `tests/test_schema.c` (new), `tests/test_headless.c` (new).
