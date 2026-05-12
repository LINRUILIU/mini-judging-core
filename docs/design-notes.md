# Design Notes

`mini-judge-core` is not intended to be a production online judge. It is a focused engineering showcase extracted from earlier course-grading tooling experience.

## Lineage

- `hw_check_exe` provided the original batch-grading workflow:
  - load a grading configuration
  - iterate over many student executables
  - run them against repeated test inputs
  - aggregate structured results
- `test_exe_running` provided the Win32 process-control experience:
  - `CreateProcess`
  - anonymous pipes for stdout/stderr capture
  - timeout and forced termination
  - handle cleanup after failure
- `txt_compare` influenced the judging model:
  - output comparison is a first-class step
  - compare rules matter
  - the MVP keeps comparison intentionally lightweight

## Why this repository exists

The old code proved the execution model worked, but it lived in large, tightly-coupled files and mixed product history with implementation details. This repository reframes the same ideas as a small, cleanly-scoped demo:

- a dedicated CLI
- explicit module boundaries
- deterministic sample cases
- portable documentation of the design choices

## Deliberate scope cuts

The MVP intentionally avoids:

- source compilation
- language runtimes beyond existing `.exe` files
- database integration
- web APIs
- sandbox claims
- checker/SPJ plugins

Those omissions are not missing polish. They are part of the repository's point: show the core execution, scheduling, judging, and reporting path without hiding behind surrounding infrastructure.

## Core architecture

The pipeline is intentionally linear:

1. `config`: read a narrow JSON schema and resolve paths
2. `scheduler`: expand `(program, testcase)` work into a bounded worker pool
3. `runner`: execute each program with Win32 process control
4. `comparator`: decide `AC` vs `WA` using a small compare-mode set
5. `engine`: aggregate case results into per-program reports
6. `reporter`: emit JSON, CSV, and HTML artifacts

## Why HTML was added

The project is meant to be shown, not only run. JSON and CSV are useful artifacts, but HTML is the presentation layer that makes the result legible in screenshots, demos, and repository browsing.
