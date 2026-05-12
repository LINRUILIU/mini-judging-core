# 25261-term Showcase Notes

This repository can be pointed at the real `25261-term` course-grading dataset.

## Why this matters

The sample `1001/1002/1003` demo proves the pipeline works, but the `25261-term` dataset is the stronger showcase:

- hundreds of existing student executables
- fixed test inputs
- teacher-provided reference outputs
- naming conventions inherited from the original grading workflow

That makes it the closest thing to a real-world replay of the older `hw_check_exe` usage model.

## Recommended first target

For this repository, the cleanest real-data showcase target is:

- program directory: `exec-vs-3-b3-2.cpp-by-win`
- input directory: `测试数据`
- answer directory: `参考答案（测试中不用，仅用于调试时参考）/3-b3`

This corresponds to the Visual Studio C++ version of the `3-b3-2` assignment and maps well to the current MVP assumptions:

- one executable per student
- stdin-driven execution
- static answer files
- text-based comparison

## Adapter strategy

The PowerShell helper `tools/Export-25261TermConfig.ps1` converts that legacy layout into the JSON schema expected by `mini-judge-core`.

It handles:

- program discovery from the legacy flat executable export
- optional roster filtering through `25261-stulist-part.txt` or `25261-stulist-full.txt`
- testcase discovery from `3-b3-*.txt`
- answer-file mapping from `3-b3-XX.txt` to `3-b3-XX.std.out.std.out`

## Expected limitations

Running the full exported VS dataset may take noticeably longer than the built-in sample because:

- there can be many executables
- some student programs may hit timeout
- the current MVP does not implement memory-limit isolation

That is acceptable for the showcase. The goal is to demonstrate the engine against authentic batch-grading data, not to claim production-grade throughput.

## Actual run result

The full `exec-vs-3-b3-2.cpp-by-win` export has already been replayed through `mini-judge-core`.

Dataset size:

- programs: `744`
- testcases per program: `32`
- total `(program, testcase)` pairs: `23808`

Observed result summary:

- `AC = 6088`
- `WA = 17720`
- `TLE = 0`
- `RE = 0`
- `OLE = 0`
- `SE = 0`

Program-level breakdown:

- `167` programs passed all `32` testcases
- `545` programs passed `0` testcases
- `32` programs partially passed
- average score: `8.18 / 32`

The generated reports are written to:

- `reports/25261-term-full/report.json`
- `reports/25261-term-full/summary.csv`
- `reports/25261-term-full/report.html`

## Reproduction command

Generate the full config:

```powershell
pwsh -File .\tools\Export-25261TermConfig.ps1 `
  -ProgramDir "D:\25261-term\exec-vs-3-b3-2.cpp-by-win" `
  -InputDir "D:\25261-term\测试数据" `
  -AnswerDir "D:\25261-term\参考答案（测试中不用，仅用于调试时参考）\3-b3" `
  -OutputConfig ".\generated\25261-term-full.json" `
  -ReportDir ".\reports\25261-term-full"
```

Run the judge:

```powershell
.\build-mingw\mini_judge.exe --config .\generated\25261-term-full.json --jobs 12 --quiet-progress
```
