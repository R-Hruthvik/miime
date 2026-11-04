# Adaptive themes Task 5 completion report

Task 5 adds full resolution diagnostics to options-aware dry-run output and opted-in MCP render telemetry. Both paths use the renderer's canonical resolution serializer, so identical clean source, annotations, request config, and crop produce byte-identical `resolution` JSON. The payload includes requested and resolved style/theme, requested and effective analysis background, sampling policy and caps, sample count, source crop and rail offset, contrast targets and hit coverage, and each annotation's sample count, halo state, unresolved state, explicit-override warning, and resolved ink.

The ordinary MCP content array remains an image entry followed by the existing telemetry text entry. Legacy requests receive the same telemetry fields as before. Requested-config serialization remains separate from ephemeral resolution; MCP get state tests assert synthesized colors and font defaults are absent after an auto-theme render.

The final integration pass also closed a Task 4 inheritance gap: ordinary render and dry-run requests now overlay supplied fields on persisted patch config. Requests that omit style/theme inherit the patched request, while explicit request-only values do not mutate that persisted config. A regression was observed failing before the merge was restored and passes with the fix.

The Task 5 review follow-up now reports zero-sample coverage as JSON `null` at both aggregate and per-annotation levels. Regressions cover an annotation outside a valid crop and an empty crop; neither is represented as full coverage.

## Final review follow-up

Desktop annotation JSON now restores root-level requested render config, and shutdown/export/save/copy preserve and consume it. Config-aware export uses options rendering for explicit new tokens, retains exact legacy export for alias-only and absent config, and restores the prior process aesthetic after alias export. Added no-GUI checks for config round-trip and alias-only legacy export equivalence.

Contrast sampling now follows actual annotation geometry with bounded, deduplicated local samples: line-like paths use their segments; rectangles, ellipses, circles, and pins use contours; direct text uses measured bounds and samples source pixels beneath it. Crop and blur operations are excluded. Direct text does not assume a theme label surface, matching the renderer. Marker contrast accounts for its rendered 0.35 alpha before halo decisions.

Focused style/schema/render tests, full `make test`, forced `make -B all`, and `git diff --check` passed after these changes. The existing legacy export/layout hashes remain unchanged.

## Verification

- `make test` — passed, including schema, bidirectional, render legacy hash, headless, MCP integration, style, and grounding tests.
- `make -B all` — passed; desktop and MCP binaries rebuilt from scratch.
- `make test_style test_render test_mcp` — passed during development.
- Luna read-only review — no concrete defects found; `git diff --check` was clean at review time.
- MCP/headless shared fixture hashes and dry-run/render diagnostics parity — passed.
- Zero-sample coverage regressions — passed for outside-crop annotation and empty crop.
