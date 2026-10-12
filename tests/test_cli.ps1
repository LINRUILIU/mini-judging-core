param(
    [Parameter(Mandatory = $true)][string]$Judge,
    [Parameter(Mandatory = $true)][string]$FixtureDir
)

$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Invoke-Judge([string[]]$CommandArgs) {
    $output = & $Judge @CommandArgs 2>&1 | Out-String
    return @{ Code = $LASTEXITCODE; Output = $output }
}

$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$root = Join-Path $tempBase ("mini_judge_cli_{0}_{1}" -f $PID, [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$heldExecutable = $null

try {
    $programDir = Join-Path $root 'programs'
    $testDir = Join-Path $root 'tests'
    $reportDir = Join-Path $root 'reports'
    [IO.Directory]::CreateDirectory($programDir) | Out-Null
    [IO.Directory]::CreateDirectory($testDir) | Out-Null
    [IO.Directory]::CreateDirectory($reportDir) | Out-Null

    $sourceNames = @('ac', 'wa', 'sleep', 're', 'out65')
    foreach ($name in $sourceNames) {
        Copy-Item -LiteralPath (Join-Path $FixtureDir "fixture_$name.exe") -Destination (Join-Path $programDir "$name.exe")
    }
    $sePath = Join-Path $programDir 'se.exe'
    Copy-Item -LiteralPath (Join-Path $FixtureDir 'fixture_ac.exe') -Destination $sePath
    # A valid PE held with an exclusive file lock fails to launch without a Windows UI dialog.
    $heldExecutable = [IO.File]::Open($sePath, [IO.FileMode]::Open,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    [IO.File]::WriteAllText((Join-Path $testDir 'case.in'), '')
    [IO.File]::WriteAllText((Join-Path $testDir 'case.out'), 'ok')

    $programs = @(
        @{ id = 'ac'; exe = 'ac.exe' },
        @{ id = 'wa'; exe = 'wa.exe' },
        @{ id = 'tle'; exe = 'sleep.exe' },
        @{ id = 're'; exe = 're.exe' },
        @{ id = 'ole'; exe = 'out65.exe' },
        @{ id = 'se'; exe = 'se.exe' }
    )
    $config = @{
        problem_id = 'all-verdicts'
        time_limit_ms = 1000
        max_output_bytes = 64
        compare_mode = 'exact'
        paths = @{
            program_dir = $programDir
            test_dir = $testDir
            answer_dir = $testDir
            report_dir = $reportDir
        }
        programs = $programs
        tests = @(@{ id = 'case'; input = 'case.in'; answer = 'case.out'; score = 7 })
    }
    $configPath = Join-Path $root 'config.json'
    $jsonText = $config | ConvertTo-Json -Depth 10
    [IO.File]::WriteAllText($configPath, $jsonText, [Text.UTF8Encoding]::new($false))

    Write-Output 'CLI phase: all verdicts'
    $normal = Invoke-Judge @('--config', $configPath, '--jobs', '3', '--quiet-progress')
    Assert-True ($normal.Code -eq 0) "Judge failed: $($normal.Output)"
    $reportPath = Join-Path $reportDir 'report.json'
    Assert-True (Test-Path -LiteralPath $reportPath -PathType Leaf) 'JSON report missing'
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json -Depth 20
    Assert-True ($report.problem_id -eq 'all-verdicts') 'Wrong problem ID'
    Assert-True ($report.total_programs -eq 6) 'Wrong total program count'
    Assert-True ($report.total_cases -eq 6) 'Wrong total case count'
    Assert-True ($report.programs.Count -eq 6) 'Wrong report program count'
    $expected = @{ ac = 'AC'; wa = 'WA'; tle = 'TLE'; re = 'RE'; ole = 'OLE'; se = 'SE' }
    foreach ($entry in $report.programs) {
        $status = $expected[$entry.program_id]
        Assert-True ($null -ne $status) "Unexpected program $($entry.program_id)"
        Assert-True ($entry.cases.Count -eq 1) "Wrong case count for $($entry.program_id)"
        Assert-True ($entry.cases[0].status -eq $status) "Wrong verdict for $($entry.program_id): $($entry.cases[0].status)"
        Assert-True ($entry.max_score -eq 7) "Wrong max score for $($entry.program_id)"
        $wantScore = if ($status -eq 'AC') { 7 } else { 0 }
        Assert-True ($entry.total_score -eq $wantScore) "Wrong score for $($entry.program_id)"
        Assert-True ($entry.passed_count -eq [int]($status -eq 'AC')) "Wrong passed count for $($entry.program_id)"
        Assert-True ($entry.failed_count -eq [int]($status -ne 'AC')) "Wrong failed count for $($entry.program_id)"
        $counter = ($status.ToLower() + '_count')
        Assert-True ($entry.$counter -eq 1) "Wrong $counter for $($entry.program_id)"
    }
    $csv = Import-Csv -LiteralPath (Join-Path $reportDir 'summary.csv')
    Assert-True ($csv.Count -eq 6) 'Wrong CSV row count'
    Assert-True (Test-Path -LiteralPath (Join-Path $reportDir 'report.html') -PathType Leaf) 'HTML report missing'

    Write-Output 'CLI phase: invalid jobs'
    foreach ($jobs in @('0', '-1', '257', '1x', '2147483648', '+1')) {
        $result = Invoke-Judge @('--config', $configPath, '--jobs', $jobs)
        Assert-True ($result.Code -ne 0) "Invalid --jobs $jobs was accepted"
    }
    foreach ($arguments in @(
        @('--config', $configPath, '--jobs'),
        @('--config', $configPath, '--jobs', '2', '--jobs', '3'),
        @('--config', $configPath, '--jobs', ' 1')
    )) {
        $result = Invoke-Judge $arguments
        Assert-True ($result.Code -ne 0) "Malformed CLI arguments were accepted: $arguments"
    }

    Write-Output 'CLI phase: report directory override'
    $overrideConfig = $config.Clone()
    $overrideConfig.paths = $config.paths.Clone()
    $overrideConfig.programs = @(@{ id = 'ac'; exe = 'ac.exe' })
    $overrideConfigPath = Join-Path $root 'override-config.json'
    $selectedReportDir = Join-Path $root '选定 报告'
    $unusedReportDir = Join-Path $root 'unused-reports'
    $overrideConfig.paths.report_dir = $unusedReportDir
    [IO.File]::WriteAllText($overrideConfigPath, ($overrideConfig | ConvertTo-Json -Depth 10),
        [Text.UTF8Encoding]::new($false))
    $override = Invoke-Judge @('--config', $overrideConfigPath, '--report-dir', $selectedReportDir, '--quiet-progress')
    Assert-True ($override.Code -eq 0) "Report directory override failed: $($override.Output)"
    Assert-True (-not (Test-Path -LiteralPath $unusedReportDir)) 'Override created the unused report directory'

    # A stale default path may name a file; the selected override must still work.
    $blockedDefault = Join-Path $root 'report-path-file'
    [IO.File]::WriteAllText($blockedDefault, 'keep this file')
    $overrideConfig.paths.report_dir = $blockedDefault
    [IO.File]::WriteAllText($overrideConfigPath, ($overrideConfig | ConvertTo-Json -Depth 10),
        [Text.UTF8Encoding]::new($false))
    $override = Invoke-Judge @('--config', $overrideConfigPath, '--report-dir', $selectedReportDir, '--quiet-progress')
    Assert-True ($override.Code -eq 0) "Unused invalid report path blocked override: $($override.Output)"
    Assert-True ([IO.File]::ReadAllText($blockedDefault) -eq 'keep this file') 'Unused default path was modified'
    $overrideReport = Get-Content -LiteralPath (Join-Path $selectedReportDir 'report.json') -Raw | ConvertFrom-Json -Depth 20
    Assert-True ($overrideReport.programs.Count -eq 1 -and $overrideReport.programs[0].cases[0].status -eq 'AC') 'Wrong override report'
    $noOverride = Invoke-Judge @('--config', $overrideConfigPath, '--quiet-progress')
    Assert-True ($noOverride.Code -ne 0) 'Selected report path that names a file was accepted'

    Write-Output 'CLI phase: report write failure'
    # A destination that is a directory must be reported as a write failure.
    $blockedReport = Join-Path $root 'blocked-reports'
    [IO.Directory]::CreateDirectory((Join-Path $blockedReport 'report.json')) | Out-Null
    $writeFailure = Invoke-Judge @('--config', $configPath, '--jobs', '1', '--report-dir', $blockedReport, '--quiet-progress')
    Assert-True ($writeFailure.Code -ne 0) 'Report write failure returned success'
    Write-Output 'mini_judge_cli_tests passed'
} finally {
    if ($null -ne $heldExecutable) { $heldExecutable.Dispose() }
    $resolved = [IO.Path]::GetFullPath($root)
    Assert-True ($resolved.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) 'Unsafe cleanup path'
    Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
}
