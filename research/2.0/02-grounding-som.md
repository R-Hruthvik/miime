# 02 — Set-of-Mark & Visual Grounding

## Core paper

- Yang et al., "Set-of-Mark Prompting Unleashes Extraordinary Visual Grounding in GPT-4V" (arXiv 2310.11441, 2023, ~936 citations). Segments image into regions (SEEM/SAM), overlays alphanumeric marks/boxes/masks; GPT-4V grounds answers to marks with no mark info leaked in text. Outperforms fully-finetuned referring-expression models on RefCOCOg zero-shot. Code: microsoft/SoM.

## SoM variants / extensions

- **US patent US20260161273A1** (filed 2024-12-05, pub 2026-06-11): SoM strategy + indexing for VLM GUI control; annotated GUI generated from DOM; iterative annotated-GUI updates after each action. Confirms SoM+DOM as the production pattern for GUI agents.
- **UGround** (arXiv 2410.05243): universal visual grounding for GUI agents; vision-only SeeAct-V; UGround trained on 10M GUI elements / 1.3M screenshots; +20% absolute over prior grounding; handles icons/widgets; cross-platform.
- **OmniParser v2** (Microsoft, 2025-02): YOLO icon detector + Florence-2 captioner + EasyOCR; +accuracy on small interactables, -60% latency; OmniParser+GPT-4o = 39.6 on ScreenSpot Pro (vs GPT-4o 0.8).
- **ScreenParse / ScreenVLM** (Microsoft, 2026-05): dense screen parsing dataset (771K screenshots, 21M elements) + 316M-param VLM decoding "ScreenTag" markup with structure-aware loss; 0.592 PageIoU vs 0.294 for larger foundation VLMs. Dense supervision transfers structural priors.
- **GUI-Actor** (NeurIPS '25): coordinate-free grounding via attention-based action head + grounding verifier; GUI-Actor-7B beats UI-TARS-72B on ScreenSpot-Pro with far fewer params.
- **deki** (2025): YOLO + attribute extraction → structured JSON + SoM image → LLM augmentation; +SSIM in code-gen, 100% vs 20% success in 10 single-step GUI tasks.

## Grounding registry design (MIIME-relevant)

- Prune DOM to 50–120 meaningful targets: visibility/viewport, occlusion culling via elementFromPoint, semantic/interactive priority, IoU-based structural dedup (>90% parent-child overlap → keep innermost unless distinct visual boundary).
- Extract computed CSS telemetry (margin, padding, border, font-size/weight/line-height, color/bg) and pre-compute WCAG contrast.
- Non-overlapping SoM tag placement: slide pill badges along perimeter; color-code by role (magenta interactive, cyan text, yellow containers).
- Dual buffers: clean source (user) vs reference map (VLM only).

## Coordinate systems

- Normalized 0–1000 scale is what most VLMs are trained to emit for bounding boxes; MIIME should accept normalized coords and scale to true resolution internally.
- In-canvas intersection watermarks at major grid crossings (e.g. 200,300) give the VLM a coordinate reference within ~50 px of any gaze point — critical for negative space.

## Static-image fallback (no DOM)

- Tier 1: classical CV (Canny + dilation) contours → SoM IDs; zero GPU.
- Tier 2: OmniParser-class ONNX (YOLO + OCR) via external HTTP endpoint; cache by image hash.

## Addressing modes (hybrid)

- `single_som_id` for tagged components (preferred).
- `som_pair` for relational spacing/flow.
- `som_group` for alignment/group hierarchy.
- `normalized_grid_box` / `normalized_point_to_point` ONLY for untagged regions / negative space.

## Implications for MIIME 2.0 spec

Already captured in spec §5 and §4.4. No changes needed; this file is the evidence trail.
