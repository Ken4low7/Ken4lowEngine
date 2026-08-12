param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,
    [double]$DurationSeconds = 1800.0,
    [string]$OutputCsv = "",
    [string]$BudgetsPath = "",
    [string]$PythonExecutable = "python",
    [switch]$StreamingStress
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path

if ([string]::IsNullOrWhiteSpace($OutputCsv)) {
    $OutputCsv = Join-Path $projectRoot "../Generated/Reliability/soak.csv"
}
if ([string]::IsNullOrWhiteSpace($BudgetsPath)) {
    $BudgetsPath = Join-Path $projectRoot "Config/ReliabilityBudgets.json"
}
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "Executable was not found: $ExecutablePath"
}
if ($DurationSeconds -le 0.0) {
    throw "DurationSeconds must be greater than zero."
}

$outputDirectory = Split-Path -Parent $OutputCsv
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$previousCsv = $env:KEN4LOW_RELIABILITY_CSV
$previousSoak = $env:KEN4LOW_SOAK_SECONDS
$previousStress = $env:KEN4LOW_STREAMING_STRESS

try {
    $env:KEN4LOW_RELIABILITY_CSV = $OutputCsv
    $env:KEN4LOW_SOAK_SECONDS = [string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0}", $DurationSeconds)
    $env:KEN4LOW_STREAMING_STRESS = if ($StreamingStress) { "1" } else { "0" }

    # The engine self-terminates at the requested soak duration so the exact same binary can be tested manually or in automation.
    $process = Start-Process -FilePath $ExecutablePath -WorkingDirectory (Split-Path -Parent $ExecutablePath) -PassThru -Wait
    if ($process.ExitCode -ne 0) {
        throw "Soak process exited with code $($process.ExitCode)."
    }
}
finally {
    $env:KEN4LOW_RELIABILITY_CSV = $previousCsv
    $env:KEN4LOW_SOAK_SECONDS = $previousSoak
    $env:KEN4LOW_STREAMING_STRESS = $previousStress
}

if (-not (Test-Path -LiteralPath $OutputCsv -PathType Leaf)) {
    throw "Reliability telemetry CSV was not created: $OutputCsv"
}

$analyzer = Join-Path $PSScriptRoot "AnalyzeReliabilityTelemetry.py"
$arguments = @(
    $analyzer,
    "--csv", $OutputCsv,
    "--budgets", $BudgetsPath,
    "--require-soak"
)
if ($StreamingStress) {
    $arguments += "--require-streaming"
}

& $PythonExecutable @arguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Soak test passed: $OutputCsv"
