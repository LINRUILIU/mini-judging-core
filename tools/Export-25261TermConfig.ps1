param(
    [Parameter(Mandatory = $true)]
    [string]$ProgramDir,

    [Parameter(Mandatory = $true)]
    [string]$InputDir,

    [Parameter(Mandatory = $true)]
    [string]$AnswerDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputConfig,

    [Parameter(Mandatory = $true)]
    [string]$ReportDir,

    [string]$RosterPath = "",
    [string]$ProblemId = "25261-term-vs-3-b3-2",
    [int]$TimeLimitMs = 3000,
    [UInt64]$MaxOutputBytes = 65536,
    [string]$CompareMode = "trim-right",
    [switch]$StageData = $true
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ProgramId {
    param([string]$FileName)

    if ($FileName -match '^(\d+-\d+)-') {
        return $Matches[1]
    }
    return [System.IO.Path]::GetFileNameWithoutExtension($FileName)
}

function Get-AllowedProgramIds {
    param([string]$Path)

    $allowed = [System.Collections.Generic.HashSet[string]]::new()
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $allowed
    }

    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }
        if ($trimmed -match '^(\d+)\s+(\d+)\b') {
            $cno = $Matches[1]
            $sno = $Matches[2]
            [void]$allowed.Add("$sno-$cno")
        }
    }
    return $allowed
}

function Resolve-RelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseUri = [System.Uri]((Resolve-Path -LiteralPath $BasePath).Path + [System.IO.Path]::DirectorySeparatorChar)
    $targetUri = [System.Uri](Resolve-Path -LiteralPath $TargetPath).Path
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

$programDirResolved = (Resolve-Path -LiteralPath $ProgramDir).Path
$inputDirResolved = (Resolve-Path -LiteralPath $InputDir).Path
$answerDirResolved = (Resolve-Path -LiteralPath $AnswerDir).Path

$outputConfigPath = [System.IO.Path]::GetFullPath($OutputConfig)
$outputDir = Split-Path -Parent $outputConfigPath
if (-not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}
if (-not (Test-Path -LiteralPath $ReportDir)) {
    New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null
}

$effectiveInputDir = $inputDirResolved
$effectiveAnswerDir = $answerDirResolved
if ($StageData) {
    $stageRoot = Join-Path $outputDir "_staged_25261_term"
    $stageInputDir = Join-Path $stageRoot "tests"
    $stageAnswerDir = Join-Path $stageRoot "answers"
    New-Item -ItemType Directory -Force -Path $stageInputDir | Out-Null
    New-Item -ItemType Directory -Force -Path $stageAnswerDir | Out-Null
    $effectiveInputDir = $stageInputDir
    $effectiveAnswerDir = $stageAnswerDir
}

$hasRoster = -not [string]::IsNullOrWhiteSpace($RosterPath)
$allowedIds = Get-AllowedProgramIds -Path $RosterPath
$programFiles = Get-ChildItem -LiteralPath $programDirResolved -Filter *.exe | Sort-Object Name
$programs = @()
foreach ($file in $programFiles) {
    $programId = Get-ProgramId -FileName $file.Name
    if ($hasRoster -and -not $allowedIds.Contains($programId)) {
        continue
    }
    $programs += [ordered]@{
        id  = $programId
        exe = $file.Name
    }
}

if ($programs.Count -eq 0) {
    throw "No programs matched the requested dataset selection."
}

$testFiles = Get-ChildItem -LiteralPath $inputDirResolved -Filter '3-b3-??.txt' | Sort-Object Name
$tests = @()
foreach ($file in $testFiles) {
    $id = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    $answerName = "$id.std.out.std.out"
    $answerPath = Join-Path $answerDirResolved $answerName
    if (-not (Test-Path -LiteralPath $answerPath)) {
        throw "Missing answer file: $answerPath"
    }
    if ($StageData) {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $effectiveInputDir $file.Name) -Force
        Copy-Item -LiteralPath $answerPath -Destination (Join-Path $effectiveAnswerDir $answerName) -Force
    }
    $tests += [ordered]@{
        id     = $id
        input  = $file.Name
        answer = $answerName
        score  = 1
    }
}

$config = [ordered]@{
    problem_id       = $ProblemId
    time_limit_ms    = $TimeLimitMs
    max_output_bytes = $MaxOutputBytes
    compare_mode     = $CompareMode
    paths            = [ordered]@{
        program_dir = Resolve-RelativePath -BasePath $outputDir -TargetPath $programDirResolved
        test_dir    = Resolve-RelativePath -BasePath $outputDir -TargetPath $effectiveInputDir
        answer_dir  = Resolve-RelativePath -BasePath $outputDir -TargetPath $effectiveAnswerDir
        report_dir  = Resolve-RelativePath -BasePath $outputDir -TargetPath $ReportDir
    }
    programs         = $programs
    tests            = $tests
}

$json = $config | ConvertTo-Json -Depth 6
Set-Content -LiteralPath $outputConfigPath -Value $json -Encoding UTF8

Write-Host "Programs: $($programs.Count)"
Write-Host "Tests: $($tests.Count)"
Write-Host "Config written to $outputConfigPath"
