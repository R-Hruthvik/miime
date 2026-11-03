# MIIME 2.0 Performance Report

Reference machine: Linux x86_64 (this build environment). GUI cold-start and idle RSS were not measurable here because GTK4 development headers are unavailable in this container; targets are carried from the v1/v2 spec.

## Measured

| Metric | Value | Method |
|---|---|---|
| `miime-mcp` startup + tools/list | < 0.01 s | `/usr/bin/time`, stdio JSON-RPC round trip |
| `miime-mcp` peak RSS | ~7 MB | `/usr/bin/time` maxrss |
| Layout engine 15-iteration pass (18 annotations) | 0.28 ms | `test_layout` `T_15_perf` |
| MCP integration test | pass | `make test_mcp` |

## Targets (from spec §1, §11, §12, §15)

| Metric | Target |
|---|---|
| Cold start (GUI) | < 200 ms |
| Idle RSS (GUI) | < 40 MB |
| Idle CPU | 0.0% (event-driven redraws) |
| Headless render (1440x900, 8 annotations) | < 500 ms |
| Grounding / SoM extraction (typical page) | < 2 s |
| Max annotations per batch | 15 |

## Notes

- `miime-mcp` has no GTK dependency and builds in environments without GTK4 dev headers.
- The GUI binary (`miime`) requires GTK4 >= 4.10 and Cairo/Pango/GdkPixbuf.
- No new features were added in Phase 6; this is a docs/polish release.
