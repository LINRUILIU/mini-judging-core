# Execution and validation contract

This document describes the P0/P1 reliability changes. The judge still runs
existing Windows executables locally. It does not provide a security sandbox,
compilation, or memory-limit enforcement.

## Input, output and verdicts

- Input is a finite byte sequence. The runner stages it in a temporary file and
  gives the child a file handle as stdin. This avoids a parent/child pipe deadlock
  when a program writes output before reading, stops reading early, or never reads.
  Interactive input and pipe-specific stdin behavior are outside this contract.
- stdout and stderr are captured separately. Only stdout is compared with the
  expected answer. `ExecResult.stderr_text` remains available to library callers.
- `max_output_bytes` is the combined stdout + stderr capture budget. Exactly N
  bytes are allowed; observing another byte produces OLE. Captured text stays
  within N bytes. `stdout_size` describes captured stdout bytes, not both streams
  and not an estimate of output discarded after termination.
- A successful execution is compared using the configured mode. Nonzero process
  exit is RE; wall timeout is TLE; excessive output is OLE; launch, I/O or supervisor
  failures are SE. A single case failure does not prevent other cases from running.
- The runner uses an elapsed wall-clock limit, not CPU time. Concurrent load can
  affect cases close to the limit. Test fixtures use comfortable margins except
  where they explicitly exercise a boundary.

## Processes and working files

Each invocation uses its own temporary working directory. Programs that previously
read auxiliary files relative to their executable directory must use explicit
paths or be adapted to the stdin/stdout contract. Executable and test-file paths
are resolved before launch.

The runner inspects executable type before launch and accepts only 32-bit or
64-bit Windows images. Missing, unreadable, malformed or legacy 16-bit images
produce SE. Launch uses a scoped thread error mode, restored on return, and the
job suppresses unhandled-exception system dialogs. This keeps ordinary launch
failures from waiting for desktop interaction; it cannot stop a program from
deliberately creating its own UI.

Processes are created suspended, assigned to a Job Object, and resumed only after
assignment succeeds. Inherited handles are restricted to the intended stdin,
stdout and stderr handles. Normal parent exit, timeout, output overflow and error
cleanup terminate remaining job processes. Closing the job is a final cleanup
backstop. This manages processes launched within that job; it does not restrict
the user's filesystem, network access or security token.

Output collection reads only available bytes and has a finite cleanup phase.
There is no blocking read-until-EOF tail. Temporary resources are released after
process handles and job resources are cleaned up.

## Configuration and CLI

| Field | Accepted values |
|---|---|
| `time_limit_ms` | Integer, 1 through 3,600,000 |
| `max_output_bytes` | Integer, 1 through 67,108,864 (64 MiB), both output streams combined |
| `tests[].score` | Nonnegative integer; sum across tests must fit in `int` |
| `--jobs` | Decimal integer, 1 through 256 |
| Program IDs / test IDs | Nonblank and unique within each list |
| Program/test lists | Nonempty; their product must fit in `int` |
| Program/input/answer paths | Existing regular files |
| Input/program/answer directories | Existing directories |
| `answer_dir` | Defaults to `test_dir` only when the field is absent |

Fractional or exponent notation is not accepted for integer fields. Negative,
overflowing, partially parsed numbers and duplicate JSON keys are rejected.
Configuration files must be valid UTF-8, at most 16 MiB and at most 64 levels deep;
paths containing NUL characters are rejected.
Malformed optional fields are errors, not requests to use their defaults.
Repeated CLI flags and missing flag values are rejected.

`LoadConfig` validates inputs and resolves paths without creating output
directories. The CLI applies `--report-dir` first, then creates and checks only
the selected destination before execution. An unused default report path is
never created and may name an existing file when an override is supplied.
Library callers prepare their chosen output directory before writing reports.

The narrow JSON parser is part of this project. The legacy file under
`third_party/nlohmann` is a placeholder and is not used by the implementation.

## Comparison

`exact` compares bytes. The two line modes normalize CRLF line endings and do not
distinguish a final line terminator from its absence. `trim-right` removes spaces
and tabs at the right of each line; `ignore-trailing-blank-lines` removes empty
trailing lines. Neither uses a string sentinel to represent a missing line, so a
literal `<missing>` has no special meaning.

In `ignore-trailing-blank-lines`, a line containing spaces or tabs is not empty.

## Scheduler and failures

The scheduler uses FIFO dispatch and at most `4 * jobs` queued tasks by default,
in addition to running tasks. Library callers can set an explicit queue capacity.
`Submit` waits when this capacity is full, providing producer backpressure. The
final report still stores one result per case; the queue bound is not a promise
that an arbitrarily large batch uses constant memory.

Every task exception is caught at the worker boundary. Accepted tasks continue,
and `Wait` rethrows the first failure after they finish. That failure is consumed
by the wait. `Cancel` discards pending tasks, wakes blocked producers and rejects
new submissions; running tasks finish. The destructor joins running work and,
without cancellation, drains accepted tasks. A task cannot submit to or wait for
its own pool because that can deadlock a bounded pool.

The engine declares its pool after all captured state, so stack unwinding joins
workers before their referenced reports and mutexes are destroyed. Failure to
allocate/store results fails the batch instead of returning an incomplete success.

## Reports and verification

Report writers check open, write, flush and close failures. The CLI prints success
only after all three writers return successfully. Reports are not published as an
atomic three-file transaction: on failure, a partial/new file or an older report
may remain, and the CLI exits nonzero. JSON control characters, CSV quotes and
HTML markup are escaped.

Tests use runtime checks that remain active with `NDEBUG`. CTest applies outer
timeouts to fault scenarios; fixture descendants also have finite lifetimes.
Integration checks inspect verdicts, scores, counts and report content. Debug and
Release are both part of the Windows CI matrix.

The fix-to-test mapping and local build results are recorded in
[the P0/P1 validation record](p0-p1-validation.md).

The historical 744-program / 23,808-case report predates these changes. It is kept
as historical evidence and is not represented as a rerun of the repaired engine.
