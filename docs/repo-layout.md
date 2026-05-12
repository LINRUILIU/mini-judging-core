# Repository Layout

This repository is intentionally split into three layers:

- `include/` and `src/`
  - the actual judging engine implementation
- `samples/` and `tests/`
  - tiny reproducible demo assets and automated checks
- `docs/` and `tools/`
  - showcase notes, design rationale, and helper scripts for replaying legacy datasets

The following directories are treated as local runtime output and are not tracked:

- `build/`
- `build-*`
- `generated/`
- `reports/`

That keeps the public repository focused on source code and reproducible instructions, while still allowing large local judging runs such as the `25261-term` replay.

