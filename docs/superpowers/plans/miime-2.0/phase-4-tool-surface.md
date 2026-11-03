# Phase 4 — Tool Surface + MCP + Self-Verification

> Goal: expose MIIME as an MCP server and implement the mandatory self-verification loop.

## Scope

Build `miime-mcp` (stdio) exposing `prepare_ux_canvas`, `render_ux_annotations`, `patch_ux_annotations`, plus a read-back tool. Wire layout telemetry warnings and the agent-side verification contract.

## Deliverables

- `src/mcp.h/.c` (NEW) or separate `src/mcp_main.c`: stdio JSON-RPC server.
- `miime-mcp` binary in Makefile.
- All three tools implemented end-to-end (grounding → layout → sketch → render).
- `get_ux_annotations` read-back tool (returns current JSON state).
- `verify_ux_render` tool returning before/after image pair + diff summary (optional but recommended).
- Telemetry warnings: high collision count, grid_box overlapping SoM element > 65%, leader crossing detected.
- MCP tool annotations (read-only hints) set correctly.

## Tasks

1. Implement minimal stdio JSON-RPC 2.0 loop (no external MCP SDK; keep deps zero).
2. Tool dispatch: parse params → validate via `schema.c` → call pipeline → serialize result.
3. `prepare_ux_canvas`: call grounding → attach Buffer B (base64 PNG) + registry records.
4. `render_ux_annotations`: deserialize → layout → sketch render → headless export → return annotated image (base64) + telemetry.
5. `patch_ux_annotations`: apply operations to in-memory state → re-layout → re-render → return.
6. `get_ux_annotations`: return current state JSON.
7. Implement telemetry warning generation in `layout.c`/`grounding.c`.
8. Add MCP tool descriptors with `readOnlyHint` etc.
9. Integration test: drive `miime-mcp` with a scripted client (Python or shell) through a full review.
10. Document MCP config (claude_desktop_config.json snippet) in README.

## Verification

- End-to-end: an agent (simulated by test script) runs prepare → render → patch → final image, with telemetry warnings surfaced.
- MCP server starts < 200 ms; idle RSS < 40 MB.
- `make test` includes MCP integration test.
- No crash on malformed input (returns JSON-RPC error).

## Gate

Full review round-trip works headlessly; telemetry warnings fire correctly on crafted inputs.

## Dependencies

Phases 1, 2, 3.

## Risks

- stdio JSON-RPC correctness (framing, base64 image size). Use newline-delimited JSON; chunk large base64? MCP allows large tool results; test with 1440×900 PNG.
- Concurrency: MCP is single-threaded request/response; document that.
- Base64 encoding in C: implement or use glib (available via GTK deps; headless binary should still link glib only).

## Files touched

`src/mcp_main.c` (new), `src/mcp.c/.h` (new), `src/schema.c` (telemetry hooks), `Makefile`, `tests/test_mcp.c` (new), `README.md`.
