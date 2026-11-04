# SDD ledger — plan: docs/superpowers/plans/2026-11-03-miime-adaptive-themes.md
Approved by user: proceed with same instructions, Luna implementers/reviewers, test/live verification, commit and push.
Base: 0134e408eda5a3521bd407b8ad4c86ecbaf0728f on codex/miime-2.1-annotation-vocabulary. Existing in-place feature-branch preference retained.
## Preflight
|Tasks|Shared contract / finding|
|---|---|
|1|Self-consistent except cross-cutting rulings below; red/green acceptance tests specified.|
|2|Self-consistent except cross-cutting rulings below; red/green acceptance tests specified.|
|3|Self-consistent except cross-cutting rulings below; red/green acceptance tests specified.|
|4|Self-consistent except cross-cutting rulings below; red/green acceptance tests specified.|
|5|Self-consistent except cross-cutting rulings below; red/green acceptance tests specified.|
|1,2|StyleConfig and annotation override flags feed resolver; preserve aliases and omitted widths.|
|1,3|Requested-only serializer must not freeze automatic values.|
|1,4|Config parser validates reset/aliases; state command and stored requests separated.|
|1,5|Requested-only get JSON vs ephemeral diagnostics.|
|2,3|Resolution must expose resolved per-annotation appearance so renderer need not repeat contrast math.|
|2,4|New style sources linked into MCP/headless and all render consumers.|
|2,5|Deterministic metadata/coverage ownership.|
|3,4|Options-aware layout API carries crop, legacy aesthetic, labels and dry run.|
|3,5|Draw and dryrun share resolver and placement stage.|
|4,5|Inherited options/reset retain typed requested config; metadata sameinput consistent.|
Ruling: Extend structs with ancillary fields needed to retain legacy aesthetic alias provenance, reset command presence, and resolved per-annotation draw tokens; prescribed signatures remain stable. These are necessary to implement existing spec behaviors, not additional features.
Ruling: Requested-only themed serialization must omit absent stroke_width as well as absent color/font_size; otherwise the old parser default width becomes an explicit override after reload and styles stop working. Legacy serialization remains byte-identical.
Ruling: Existing explicit font_size:0 keeps legacy fallback size (24 rendered pixels), not an invisible zero-size label; absence chooses style font size. Presence must survive roundtrip.
Ruling: Extend layout with an options-aware metrics adapter if needed for new absolute font sizes, retaining existing public legacy wrappers and hashes.
Ruling: No-options desktop paths remain legacy; saved JSON with explicit new options must render through the same options-aware path wherever that JSON is consumed.
Ruling (final review): Desktop annotation files retain root-level requested render config on `MiimeWindow`; save, save-as, copy, annotation export, and `--annotations-out` preserve it. Explicit style/theme/background requests use options-aware export. Alias-only requests use the exact legacy export with the stored aesthetic temporarily applied and restored; absent config stays on the unchanged legacy route.
Ruling (final review): Local contrast samples follow annotation geometry: line-like paths use bounded, deduplicated segment samples; rectangles and ellipses use contour samples; direct text uses measured text bounds and source pixels beneath the text. Crop and blur are excluded from contrast diagnostics. Direct text has no assumed label surface because the renderer draws it over source pixels.
Ruling (final review): Marker contrast uses the rendered 0.35 opacity before deciding whether halo fallback is needed.
Ruling (final review correction): Labeled annotations retain independent contour and label-text sample/hit counts. Halo-rescued shape hits are tracked separately, and unresolved status requires both shape coverage (after halo) and text coverage to meet their own targets. Brackets sample the renderer's three connected segments, bounded and deduplicated to 64 source pixels.
Task 1: complete (0134e40..8ba73c0, review clean; full make test passes).
Task 2 BASE: 8ba73c0.

Task 2: complete locally; resolver, palette accessors, source linkage, tests, and report added. `make test_style`, full `make test`, `make all`, and `git diff --check` passed (full suite preceded final test-only additions; focused style suite reran afterward). Awaiting commit hash.

Task 3: complete locally. Added the exact options-aware export and layout/dry-run APIs, a metrics-aware layout adapter, request-scoped resolved annotation copies, local neutral dual halos, style-specific geometry and absolute Pango text sizes, opaque/default and explicit label surfaces, crop/rail diagnostics, and render-dependent test linkage. Options-aware export routes labeled stacks through automatic layout. Existing no-options export/layout paths retain their previous results.

Task 3 verification: `make test_render`, full `make test`, and `make -B all` pass. Compared raw Cairo pixel hashes for a labeled fixture against the pre-Task3 `HEAD` renderer: export `85eed7a48c2db46b65cf95ec9dc694f8828cdcd7c5b6730afd5837a06586866e`; layout `266fc1d39c2799982b51f5733378e0896e0b0ee4389545f388509438ae3d9c3c`; both match and are pinned in `test_render_legacy_hashes_without_options`.

Task 3 integration rulings: `MiimeRenderOptions.resolution` is borrowed and reused when supplied; otherwise each options-aware request resolves once from the clean image and effective crop. Temporary annotation copies borrow `blur_data` and never transfer/free it. The options-aware layout path keeps label placement/metrics shared by drawing and dry-run, reports crop coordinates and the 300px rail offset, and draws white outer then black inner halos on the annotation's actual geometry. A NULL options pointer delegates to the original renderer.

Task 4: complete locally and committed. MCP now validates and merges requested config independently from annotation state, supports patch inheritance/reset, emits requested-only state, and clears settings on prepare. MCP/headless opted-in requests share the options-aware layout renderer. The CLI forwards `--annotations` to headless config parsing. Legacy alias-only requests retain the 2.1 path; alias mapping and conflict diagnostics are covered under opt-in.

Task 4 verification: RED observed for the missing MCP schema and headless options-renderer hash mismatch. `make test_mcp`, `make test_headless`, full `make test`, forced `make -B all`, and `git diff --check` pass. MCP integration compares identical auto-theme output through MCP, the headless API, and the desktop CLI.

Final review: all four concrete findings in `review-full.diff` are addressed locally. Focused style/schema/render tests, full `make test`, forced `make -B all`, and `git diff --check` pass. Legacy render golden hashes remain covered by `test_render_legacy_hashes_without_options`; saved alias-only config has a separate no-GUI legacy export equivalence regression. No push performed.

Final review correction verification: the two mixed-coverage and bracket-geometry regressions pass in `make test_style`; full `make test`, `make -B all`, and `git diff --check` pass. A first integration run used stale desktop objects after the public diagnostics struct grew; forcing the complete rebuild refreshed all consumers, after which MCP/headless/CLI pixel parity passed.
