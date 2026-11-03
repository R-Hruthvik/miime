# Changelog

All notable changes to MIIME are documented here.

## [2.1.0] - 2026-11-03

### Added
- Full MCP input schemas for all five tools, including annotation primitives, style vocabulary, layout controls, and dry-run placement output.
- Annotation labels, margin-rail and in-canvas layouts, Set-of-Mark targeting, freehand strokes, tapered widths, and additional annotation styling.
- Legacy render pixel goldens and repeated-render determinism coverage.
- README reference for all MCP tools and the 2.1 annotation vocabulary.

## [2.0.0] - 2026-11-03

### Added
- Set-of-Mark grounding pipeline (`prepare_ux_canvas`) with reference map and grounding registry.
- Declarative annotation batch rendering (`render_ux_annotations`) and patching (`patch_ux_annotations`).
- Collision-free layout engine (margin rail / in-canvas floating, force relaxation, leader routing).
- Deterministic seeded sketch renderer (`rough_sketch` default, `clean_technical` opt-in).
- Headless Cairo pipeline and `miime --headless` CLI.
- `miime-mcp` stdio MCP server (no GTK dependency).
- Bi-directional human↔agent sync via `--annotations` / `--annotations-out`.
- Color semantics (critical / warning / suggestion / positive) and hybrid addressing rules.

### Changed
- Annotation state is now the single source of truth (mouse tools are one producer among several).
- README, desktop integration, and install targets updated for v2.

### Preserved
- v1 viewer features, keybindings, and memory/performance constraints (cold start < 200 ms, idle RSS < 40 MB, zero idle CPU).

## [1.0.0] - 2026-09-28

### Added
- Initial GTK4 image viewer and annotation tool (pen, highlighter, shapes, text, blur, crop).
- Export to PNG/JPEG and clipboard.
- Desktop integration and `make install` / `make uninstall`.
