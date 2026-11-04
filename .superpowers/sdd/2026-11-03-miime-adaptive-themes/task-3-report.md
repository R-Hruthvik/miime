# Task 3 report — style-aware shared renderer

Implemented the options-aware rendering API in `src/render.h/.c` and added a metrics-aware layout adapter in `src/layout.h/.c`.

## Behavior

- `miime_render_export_with_options` resolves once from the clean source and crop, applies resolved per-annotation appearance to temporary copies, respects explicit ink/fill/font/width/radius/surface overrides, and routes labeled exports through automatic layout.
- `miime_render_annotations_with_options` shares one resolution and one placement pass across rendering and dry-run JSON. It uses absolute Pango pixel sizes and measured wrapped block dimensions, draws opaque default label surfaces while honoring explicit surface alpha, and reports source crop coordinates plus the rail offset.
- Contrast fallback paints a white outer and black inner outline on the same annotation geometry. Sketch randomness remains request deterministic. Crop, z/marker ordering, pin numbering, annotation-stack contents and borrowed blur buffers are preserved.
- Existing no-options entry points retain the original compatibility path. Renderer-dependent test targets now link `src/style.c`.

## Tests and compatibility

Added tests for resolved theme ink and explicit-color precedence, source and stack immutability, blur buffer ownership, crop/rail source mapping, shared dry-run resolution metadata, all style width/radius/font tokens, actual path pixels and wrapped Pango metrics, all five themes, source alpha preservation, label-surface opacity, and dual-halo pixels.

Pinned raw-pixel hashes match the pre-Task3 renderer for a labeled fixture:

- Export: `85eed7a48c2db46b65cf95ec9dc694f8828cdcd7c5b6730afd5837a06586866e`
- Layout: `266fc1d39c2799982b51f5733378e0896e0b0ee4389545f388509438ae3d9c3c`

`make test_render`, full `make test`, and forced `make -B all` passed. The final renderer diff was checked against the baseline hashes before committing.
