# 01 — Competitive / Prior-Art Landscape

## Agent-facing visual annotation tools (2026)

- **Agentation** (Medium): visual feedback tool for AI coding agents; adds an annotation layer to a local project so reviewers can mark UI issues the agent then reads.
- **Markup** (getmarkup.dev): Chrome extension; pin notes to exact DOM elements with CSS selectors; export a structured markdown brief; works on localhost/staging/file://; no backend.
- **Pincue** (getpincue.com): Chrome extension; click element → auto-screenshot with green bounding box → note with severity → export markdown; up to 15 notes/session; smart selectors.
- **onUI** (onui.onllm.dev): Annotate mode (element-level, multi-select) + Draw mode (rect/ellipse regions for spacing/layout); local MCP (`onui_get_report`, `onui_search_annotations`); pair-programming loop with Claude Code/Codex.
- **Screenote** (screenote.ai): agent captures → human marks → agent retrieves cropped regions; round-trip, no duplicate uploads.
- **ViewGraph** (github.com/sourjya/viewgraph): browser extension + MCP; structured DOM captures, 41 MCP tools, 97% token reduction; human annotations bundled with DOM capture + enrichment.
- **BugShot / ComiRadar** (github.com/sixtdreanight/bugshot): screenshot → annotate (pins/arrows/rects/freehand) → MCP read-back; UIA element tree on Windows; Markdown/JSON/PNG export.
- **agent-canvas** (github.com/edrouhardmicrosoft/agent-canvas-skills): spec-driven design QA; annotated screenshots + fix lists; interactive browser badges; GitHub issue creation.
- **UI Verify** (uiverify.ai): visual regression testing for Playwright/Storybook/Vitest/Bun; AI judge sorts intended vs regression; MCP feedback to coding agent.
- **mcpware/ui-annotator-mcp**: MCP server that annotates any web page with hover labels on every element (no screenshots/selectors).

## MCP servers for screen perception / automation

- **OmniParser MCP** (cute-aaa/OmniParser-MCP): thin MCP proxy over microsoft OmniParser (YOLOv9 + Florence-2 + EasyOCR); `parse_screen` returns structured elements + SoM image.
- **OmniMCP** (OpenAdaptAI): perceive-plan-act loop; OmniParser + LLM planner + pynput input; visual logs.
- **omniparser-autogui-mcp** (NON906): analyzes screen with OmniParser and operates GUI; Windows.
- **kira-mcp**: local YOLO-based UI detection (OmniParser-v2 icon detector) + OCR + hash caching + full keyboard/mouse/screen/clipboard control; `perceive_screen` returns annotated image + `{id,bbox,cx,cy,confidence}`.
- **glovebox-mcp**: sandboxed X11 apps; `parse_screen` vision grounding (OmniParser/Tesseract/none) → numbered SoM image + element JSON.
- **desktop-control-mcp**: OmniParser v2 detection + EasyOCR; mandatory detect→find→click→verify loop.

## Bug-report MCP comparison

- marklayer.app guide "Bug Report MCP Servers Compared: 5 Tools (2026)" — survey of MCP servers that let coding agents read bug reports/annotations instead of pasting screenshots.

## Key findings for MIIME 2.0

1. The field converges on: **screenshot + numbered SoM image + element JSON** as the standard agent perception payload. MIIME must emit exactly this shape.
2. Human→agent feedback loops (Markup/Pincue/onUI/Screenote/ViewGraph/BugShot) all use **DOM selectors or cropped regions**; MIIME differentiates by supporting *arbitrary screenshots* (no DOM) via CV/ML grounding and by being the *rendering engine* (sketch style + collision layout) rather than a Chrome extension.
3. Bi-directional round-trips are the differentiator: agent annotates → human edits → agent patches. Most tools are one-directional.
4. Performance: OmniParser ~50–200 ms GPU / 300–800 ms CPU; MIIME must keep grounding external and cache by image hash to stay under 40 MB RSS.
5. MCP is the integration surface of choice in 2026; a stdio MCP binary is mandatory.
6. Visual regression (UI Verify) is adjacent but distinct — keep MIIME focused on interactive review, not CI gating.
