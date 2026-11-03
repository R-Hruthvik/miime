# Phase 6 — Polish, Docs, Demo, Release

> Goal: ship MIIME 2.0 as a documented, installable, demoed release.

## Scope

Final verification, documentation, packaging, and a recorded end-to-end demo. No new features.

## Deliverables

- README v2 (features, MCP setup, headless usage, keybindings, dependencies).
- `miime.desktop` updated with new MIME types / keywords if needed.
- `make install` / `make uninstall` verified on a clean machine/container.
- End-to-end demo: agent reviews a live URL → annotated image → human edits → agent patches → final. Recorded (screen capture) and linked in README.
- Performance report: cold start, idle RSS, headless render time, grounding time on a reference machine.
- CHANGELOG entry for 2.0.
- Tag `v2.0.0`.

## Tasks

1. Run full verification loop (build, type check, lint, tests with coverage, security grep, diff review) — use `verification-loop` skill.
2. `desktop-file-validate` on `.desktop`.
3. Test install/uninstall in a container (Debian + Fedora).
4. Record demo.
5. Write performance report into `docs/perf-2.0.md`.
6. Update `plan.md` / `README.md` version references.
7. Tag release.

## Verification

- All tests green on two distros.
- Install/uninstall clean (no leftover files).
- Demo reproducible by a third party following README.
- Performance targets met (§1 success criteria).

## Gate

Release checklist signed off; tag pushed.

## Dependencies

Phase 5.

## Risks

- Distro-specific GTK4 version differences; test on GTK 4.10+ and 4.14+.
- Demo reproducibility depends on network (live URL); include an offline fixture fallback.

## Files touched

`README.md`, `CHANGELOG.md` (new), `docs/perf-2.0.md` (new), `Makefile`, `miime.desktop`, git tag.
