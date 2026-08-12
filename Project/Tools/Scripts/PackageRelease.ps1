param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path,
    [string]$ExecutablePath = "",
    [string]$OutputDirectory = "",
    [ValidateSet("Debug", "Development", "Shipping")]
    [string]$BuildProfile = "Shipping",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $ProjectRoot "../Generated/outputs/x64/Release/Ken4lowEngine.exe"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $ProjectRoot "../Generated/ReleasePackages"
}

$resourcesPath = Join-Path $ProjectRoot "Resources"
$stagingRoot = Join-Path $OutputDirectory "Ken4lowEngine-$BuildProfile"
$symbolsRoot = Join-Path $OutputDirectory "Ken4lowEngine-$BuildProfile-Symbols"
$packageZip = "$stagingRoot.zip"
$symbolsZip = "$symbolsRoot.zip"

if (-not (Test-Path -LiteralPath $resourcesPath -PathType Container)) {
    throw "Resources directory was not found: $resourcesPath"
}

if ($DryRun) {
    Write-Host "Release packaging dry run"
    Write-Host "  ProjectRoot: $ProjectRoot"
    Write-Host "  Executable: $ExecutablePath"
    Write-Host "  Resources: $resourcesPath"
    Write-Host "  Package: $packageZip"
    Write-Host "  Symbols: $symbolsZip"
    Write-Host "  BuildProfile: $BuildProfile"
    exit 0
}

if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "Release executable was not found: $ExecutablePath"
}

foreach ($path in @($stagingRoot, $symbolsRoot)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

Copy-Item -LiteralPath $ExecutablePath -Destination $stagingRoot
Copy-Item -LiteralPath $resourcesPath -Destination (Join-Path $stagingRoot "Resources") -Recurse

$binaryDirectory = Split-Path -Parent $ExecutablePath
Get-ChildItem -LiteralPath $binaryDirectory -Filter "*.dll" -File -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $stagingRoot
}

# Symbols are archived separately so shipping users receive small packages while crash dumps remain symbolizable internally.
Get-ChildItem -LiteralPath $binaryDirectory -Filter "*.pdb" -File -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $symbolsRoot
}

$manifestEntries = Get-ChildItem -LiteralPath $stagingRoot -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        Path = [IO.Path]::GetRelativePath($stagingRoot, $_.FullName).Replace("\", "/")
        Size = $_.Length
        Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$manifest = [ordered]@{
    Format = "Ken4lowReleasePackage"
    Version = 1
    BuildProfile = $BuildProfile
    CreatedUtc = [DateTime]::UtcNow.ToString("o")
    Files = @($manifestEntries)
}
$manifestPath = Join-Path $stagingRoot "PackageManifest.json"
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

if (Test-Path -LiteralPath $packageZip) { Remove-Item -LiteralPath $packageZip -Force }
if (Test-Path -LiteralPath $symbolsZip) { Remove-Item -LiteralPath $symbolsZip -Force }
Compress-Archive -Path (Join-Path $stagingRoot "*") -DestinationPath $packageZip -CompressionLevel Optimal

$symbolFiles = @(Get-ChildItem -LiteralPath $symbolsRoot -File -Recurse)
if ($symbolFiles.Count -gt 0) {
    Compress-Archive -Path (Join-Path $symbolsRoot "*") -DestinationPath $symbolsZip -CompressionLevel Optimal
}

Write-Host "Release package created: $packageZip"
if ($symbolFiles.Count -gt 0) {
    Write-Host "Symbol package created: $symbolsZip"
} else {
    Write-Warning "No PDB files were found; symbol package was not created."
}
