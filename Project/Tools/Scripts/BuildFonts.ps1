param(
    [string]$ProjectDir = ".",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [switch]$Force,
    [int]$BuildVersion = 1
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BuildAssetCommon.ps1")

$fontSize = 48
$atlasWidth = 1024
$atlasHeight = 1024

function Invoke-FontConverter {
    param(
        [string]$ExePath,
        [string]$FontPath,
        [string]$OutDir,
        [string]$CharsetFile,
        [string]$LogPath
    )

    $stdoutLog = [System.IO.Path]::ChangeExtension($LogPath, ".out.log")
    $stderrLog = [System.IO.Path]::ChangeExtension($LogPath, ".err.log")
    foreach ($path in @($stdoutLog, $stderrLog, $LogPath)) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
    }

    $argList = @(
        $FontPath,
        "-out", $OutDir,
        "-size", "$fontSize",
        "-atlasWidth", "$atlasWidth",
        "-atlasHeight", "$atlasHeight",
        "-charsetFile", $CharsetFile
    )

    $process = Start-Process -FilePath $ExePath -ArgumentList $argList -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog

    if (Test-Path -LiteralPath $stdoutLog) { Get-Content -LiteralPath $stdoutLog | Add-Content -LiteralPath $LogPath }
    if (Test-Path -LiteralPath $stderrLog) { Get-Content -LiteralPath $stderrLog | Add-Content -LiteralPath $LogPath }
    return $process.ExitCode
}

function Move-FontOutputs {
    param(
        [string]$SourceDir,
        [string]$TextureOutDir,
        [string]$MetaOutDir
    )

    Ensure-Directory -Path $TextureOutDir
    Ensure-Directory -Path $MetaOutDir
    Get-ChildItem -Path $SourceDir -File -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object {
        Move-Item -LiteralPath $_.FullName -Destination (Join-Path $TextureOutDir $_.Name) -Force
    }
    Get-ChildItem -Path $SourceDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Extension.ToLowerInvariant() -in @(".json", ".txt", ".pgm")
    } | ForEach-Object {
        Move-Item -LiteralPath $_.FullName -Destination (Join-Path $MetaOutDir $_.Name) -Force
    }
}

function Get-FontOutputPaths {
    param(
        [string]$TextureOutDir,
        [string]$MetaOutDir
    )

    $paths = @()
    $paths += Get-ChildItem -Path $TextureOutDir -File -Filter *.png -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
    $paths += Get-ChildItem -Path $MetaOutDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -ne "font.buildmeta.json" -and $_.Extension.ToLowerInvariant() -in @(".json", ".txt", ".pgm")
    } | Select-Object -ExpandProperty FullName
    return $paths
}

function Test-FontBuildRequired {
    param(
        [string]$MetaPath,
        [string]$FontRelativePath,
        [string]$CharsetRelativePath,
        [string]$FontHash,
        [string]$CharsetHash,
        [int]$BuildVersion,
        [bool]$Force
    )

    if ($Force) { return "Force rebuild" }
    if (!(Test-Path -LiteralPath $MetaPath -PathType Leaf)) { return "Missing metadata" }

    $meta = Read-BuildMeta -MetaPath $MetaPath
    if ($null -eq $meta) { return "Broken metadata" }
    if ([int]$meta.BuildVersion -ne $BuildVersion) { return "BuildVersion changed" }
    if ([string]$meta.AssetType -ne "Font") { return "AssetType changed" }
    if ([string]$meta.FontPath -ne $FontRelativePath) { return "Font path changed" }
    if ([string]$meta.CharsetPath -ne $CharsetRelativePath) { return "Charset path changed" }
    if ([string]$meta.FontSha256 -ne $FontHash) { return "Font changed" }
    if ([string]$meta.CharsetSha256 -ne $CharsetHash) { return "Charset changed" }
    if ([int]$meta.FontSize -ne $fontSize) { return "FontSize changed" }
    if ([int]$meta.AtlasWidth -ne $atlasWidth) { return "AtlasWidth changed" }
    if ([int]$meta.AtlasHeight -ne $atlasHeight) { return "AtlasHeight changed" }

    foreach ($relativeOutputPath in @($meta.OutputPaths)) {
        $outputPath = Join-Path $ProjectDir ([string]$relativeOutputPath).Replace('/', '\')
        if (!(Test-Path -LiteralPath $outputPath -PathType Leaf)) {
            return "Missing output"
        }
    }
    return $null
}

function Build-FontVariant {
    param(
        [string]$VariantName,
        [string]$FontPath,
        [string]$CharsetFile,
        [string]$TempDir,
        [string]$TextureOutDir,
        [string]$MetaOutDir,
        [string]$ConverterPath,
        [int]$BuildVersion,
        [bool]$Force
    )

    Ensure-Directory -Path $TempDir
    Ensure-Directory -Path $TextureOutDir
    Ensure-Directory -Path $MetaOutDir

    $metaPath = Join-Path $MetaOutDir "font.buildmeta.json"
    $fontRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $FontPath
    $charsetRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $CharsetFile
    $fontHash = Get-FileSha256 -Path $FontPath
    $charsetHash = Get-FileSha256 -Path $CharsetFile

    $buildReason = Test-FontBuildRequired -MetaPath $metaPath -FontRelativePath $fontRelativePath `
        -CharsetRelativePath $charsetRelativePath -FontHash $fontHash -CharsetHash $charsetHash `
        -BuildVersion $BuildVersion -Force $Force

    if ($null -eq $buildReason) {
        Write-Host "[BuildFonts] Skip: $VariantName"
        return $false
    }

    Write-Host "[BuildFonts] Build : $VariantName"
    Write-Host "[BuildFonts] Reason: $buildReason"

    # 専用出力フォルダ内の旧生成物を消し、廃止されたページが残らないようにする。
    Get-ChildItem -Path $TextureOutDir -File -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem -Path $MetaOutDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -ne "font.buildmeta.json" -and $_.Extension.ToLowerInvariant() -in @(".json", ".txt", ".pgm")
    } | Remove-Item -Force
    Get-ChildItem -Path $TempDir -File -ErrorAction SilentlyContinue | Remove-Item -Force

    $logPath = Join-Path $MetaOutDir ($VariantName.ToLowerInvariant() + "_build.log")
    $exitCode = Invoke-FontConverter -ExePath $ConverterPath -FontPath $FontPath `
        -OutDir $TempDir -CharsetFile $CharsetFile -LogPath $logPath
    if ($exitCode -ne 0) {
        throw "$VariantName font conversion failed. ExitCode=$exitCode Log=$logPath"
    }

    Move-FontOutputs -SourceDir $TempDir -TextureOutDir $TextureOutDir -MetaOutDir $MetaOutDir
    $outputPaths = Get-FontOutputPaths -TextureOutDir $TextureOutDir -MetaOutDir $MetaOutDir | ForEach-Object {
        Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $_
    }
    if (@($outputPaths).Count -eq 0) {
        throw "$VariantName font converter created no output files."
    }

    $meta = [ordered]@{
        BuildVersion    = $BuildVersion
        AssetType       = "Font"
        Variant         = $VariantName
        FontPath        = $fontRelativePath
        CharsetPath     = $charsetRelativePath
        FontSha256      = $fontHash
        CharsetSha256   = $charsetHash
        FontSize        = $fontSize
        AtlasWidth      = $atlasWidth
        AtlasHeight     = $atlasHeight
        OutputPaths     = @($outputPaths)
    }
    Write-BuildMeta -Meta $meta -MetaPath $metaPath
    return $true
}

try {
    $ProjectDir = (Resolve-Path $ProjectDir).Path
}
catch {
    throw "ProjectDir not found: $ProjectDir"
}

$rootDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectDir ".."))
$generatedRoot = Join-Path $rootDir "Generated"
$candidates = @(
    (Join-Path $generatedRoot "Bin\FontConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Platform\$Configuration\FontConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Configuration\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Platform\$Configuration\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Configuration\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\FontConverter\FontConverter.exe"),
    (Join-Path $ProjectDir "Tools\FontConverter\$Platform\$Configuration\FontConverter.exe")
)

$fontConverterExe = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $fontConverterExe) {
    throw "FontConverter.exe not found. Candidates:`n$($candidates -join "`n")"
}

$fontSourceDir = Join-Path $ProjectDir "Resources\Fonts\Sources"
$charsetDir = Join-Path $ProjectDir "Resources\Fonts\Charsets"
$fontPath = Join-Path $fontSourceDir "DotGothic16-Regular.ttf"
$latinCharsetFile = Join-Path $charsetDir "LatinCharset.txt"
$jpCharsetFile = Join-Path $charsetDir "JPCharset.txt"
foreach ($requiredPath in @($fontPath, $latinCharsetFile, $jpCharsetFile)) {
    if (!(Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required font input not found: $requiredPath"
    }
}

$tempRoot = Join-Path $generatedRoot "Fonts"
$textureFontRoot = Join-Path $ProjectDir "Resources\Textures\Sources\UI\Font"
$fontMetaRoot = Join-Path $ProjectDir "Resources\Fonts\Compiled"

$builtCount = 0
if (Build-FontVariant -VariantName "Latin" -FontPath $fontPath -CharsetFile $latinCharsetFile `
    -TempDir (Join-Path $tempRoot "Latin") -TextureOutDir (Join-Path $textureFontRoot "Latin") `
    -MetaOutDir (Join-Path $fontMetaRoot "Latin") -ConverterPath $fontConverterExe `
    -BuildVersion $BuildVersion -Force $Force) {
    $builtCount++
}
if (Build-FontVariant -VariantName "JP" -FontPath $fontPath -CharsetFile $jpCharsetFile `
    -TempDir (Join-Path $tempRoot "JP") -TextureOutDir (Join-Path $textureFontRoot "JP") `
    -MetaOutDir (Join-Path $fontMetaRoot "JP") -ConverterPath $fontConverterExe `
    -BuildVersion $BuildVersion -Force $Force) {
    $builtCount++
}

if (Test-Path -LiteralPath $tempRoot -PathType Container) {
    Remove-Item -LiteralPath $tempRoot -Force -Recurse -ErrorAction SilentlyContinue
}

Write-Host "[BuildFonts] Completed. Built=$builtCount UpToDate=$((2 - $builtCount))"
exit 0
