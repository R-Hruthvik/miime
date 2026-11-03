# 05 — MCP & Bi-Directional Feedback

## MCP as the integration surface (2026)

- MCP (Model Context Protocol) is the standard tool interface for coding agents (Claude Code, Cursor, Codex, Cline, Windsurf). MIIME v2 ships a stdio MCP binary (`miime-mcp`) exposing `prepare_ux_canvas`, `render_ux_annotations`, `patch_ux_annotations`.
- VS Code (Jan 2026) supports MCP Apps — agents can display interactive UIs; relevant for the bi-directional viewer.
- Tool annotations (read-only/destructive hints) are metadata; MIIME tools are read-only renderers except patch (mutating in-memory state).

## Bi-directional annotation patterns in the wild

- **Screenote**: agent captures → human marks → agent retrieves cropped regions; clean round-trip.
- **Markup / Pincue**: human pins to DOM selectors → structured markdown brief → agent acts in one pass; "less credit creep" (structured feedback avoids re-prompting).
- **onUI**: annotate/draw modes + local MCP `onui_get_report`/`onui_search_annotations`; continuous pair-programming loop (annotate → agent reads → fixes → re-annotate).
- **ViewGraph**: human annotations bundled with full DOM capture + enrichment → `Send to Agent`; extension works standalone too.
- **BugShot/ComiRadar**: MCP tools `list_annotations`, `get_screenshot`, `resolve_annotation`, `get_context` (structured Markdown for AI).
- **agent-canvas**: interactive browser badges + `canvas-edit` (annotation toolbar overlay) + `canvas-apply` (converts visual edits to code changes) + `canvas-verify` (before/after comparison).

## Patterns MIIME should adopt

1. **JSON layer as single source of truth** (all tools above converge on this).
2. **Agent reads via tool, not paste** (MCP read-back; avoids context loss).
3. **Structured, severity-tagged, selector/element-anchored notes** → MIIME uses SoM ids + normalized coords instead of CSS selectors (works on arbitrary screenshots).
4. **Round-trip patching**: human edit → JSON delta → agent patch (MIIME's `patch_ux_annotations`).
5. **Before/after verification** (agent-canvas `canvas-verify`, UI Verify) → MIIME's self-verification loop + optional human verification pass.
6. **No auto-write sidecars by default** (MIIME v1 constraint); opt-in `--annotations-out` for harness use.

## Implications for MIIME 2.0

- MCP binary is mandatory, not optional, for v2 adoption.
- Expose a `get_annotations`/`get_context`-style read-back tool in addition to the 3 render tools, so agents can re-read state after human edits.
- Support a `verify` mode returning before/after image pair + diff summary.
- Keep all tools read-only-friendly where possible; mark render tools as non-destructive (they produce new images, not mutate source files).
- Bi-directional sync (§10 of spec) is the killer feature; prioritize in Phase 5.
