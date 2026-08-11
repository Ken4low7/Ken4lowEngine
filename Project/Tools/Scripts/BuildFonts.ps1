param(
    [string]$ProjectDir = ".",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [switch]$Force,
    [switch]$DisableDdc,
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

function Clear-FontOutputs {
    param(
        [string]$TextureOutDir,
        [string]$MetaOutDir
    )

    Get-ChildItem -Path $TextureOutDir -File -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem -Path $MetaOutDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -ne "font.buildmeta.json" -and $_.Extension.ToLowerInvariant() -in @(".json", ".txt", ".pgm")
    } | Remove-Item -Force
}

function Restore-FontDdc {
    param(
        [string]$DdcEntryDirectory,
        [string]$TextureOutDir,
        [string]$MetaOutDir
    )

    $cacheTextureDir = Join-Path $DdcEntryDirectory "Textures"
    $cacheMetaDir = Join-Path $DdcEntryDirectory "Metadata"
    $cachedTextureFiles = @(Get-ChildItem -Path $cacheTextureDir -File -ErrorAction SilentlyContinue)
    $cachedMetaFiles = @(Get-ChildItem -Path $cacheMetaDir -File -ErrorAction SilentlyContinue)
    if (($cachedTextureFiles.Count + $cachedMetaFiles.Count) -eq 0) {
        return [pscustomobject]@{ Hit = $false; Bytes = [int64]0 }
    }

    Clear-FontOutputs -TextureOutDir $TextureOutDir -MetaOutDir $MetaOutDir
    Ensure-Directory -Path $TextureOutDir
    Ensure-Directory -Path $MetaOutDir
    $bytes = [int64]0
    foreach ($file in $cachedTextureFiles) {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $TextureOutDir $file.Name) -Force
        $bytes += [int64]$file.Length
    }
    foreach ($file in $cachedMetaFiles) {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $MetaOutDir $file.Name) -Force
        $bytes += [int64]$file.Length
    }
    return [pscustomobject]@{ Hit = $true; Bytes = $bytes }
}

function Store-FontDdc {
    param(
        [string]$DdcEntryDirectory,
        [string]$TextureOutDir,
        [string]$MetaOutDir
    )

    if (Test-Path -LiteralPath $DdcEntryDirectory -PathType Container) {
        Remove-Item -LiteralPath $DdcEntryDirectory -Recurse -Force
    }
    $cacheTextureDir = Join-Path $DdcEntryDirectory "Textures"
    $cacheMetaDir = Join-Path $DdcEntryDirectory "Metadata"
    Ensure-Directory -Path $cacheTextureDir
    Ensure-Directory -Path $cacheMetaDir

    $bytes = [int64]0
    Get-ChildItem -Path $TextureOutDir -File -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $cacheTextureDir $_.Name) -Force
        $bytes += [int64]$_.Length
    }
    Get-ChildItem -Path $MetaOutDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -ne "font.buildmeta.json" -and $_.Extension.ToLowerInvariant() -in @(".json", ".txt", ".pgm")
    } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $cacheMetaDir $_.Name) -Force
        $bytes += [int64]$_.Length
    }
    return $bytes
}

function Test-FontBuildRequired {
    param(
        [string]$MetaPath,
        [string]$FontRelativePath,
        [string]$CharsetRelativePath,
        [string]$FontHash,
        [string]$CharsetHash,
        [string]$BuildKey,
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
    if ([string]$meta.BuildKey -ne $BuildKey) { return "BuildKey changed" }

    foreach ($relativeOutputPath in @($meta.OutputPaths)) {
        $outputPath = Join-Path $ProjectDir ([string]$relativeOutputPath).Replace('/', '\')
        if (!(Test-Path -LiteralPath $outputPath -PathType Leaf)) {
            return "Missing output"
        }
    }
    return $null
}

function Write-FontBuildMeta {
    param(
        [string]$MetaPath,
        [string]$VariantName,
        [string]$FontRelativePath,
        [string]$CharsetRelativePath,
        [string]$FontHash,
        [string]$CharsetHash,
        [string]$BuildKey,
        [string[]]$OutputPaths,
        [int]$BuildVersion
    )

    $meta = [ordered]@{
        BuildVersion    = $BuildVersion
        AssetType       = "Font"
        BuildKey        = $BuildKey
        Variant         = $VariantName
        FontPath        = $FontRelativePath
        CharsetPath     = $CharsetRelativePath
        FontSha256      = $FontHash
        CharsetSha256   = $CharsetHash
        FontSize        = $fontSize
        AtlasWidth      = $atlasWidth
        AtlasHeight     = $atlasHeight
        OutputPaths     = @($OutputPaths)
    }
    Write-BuildMeta -Meta $meta -MetaPath $MetaPath
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
        [bool]$Force,
        [bool]$DisableDdc
    )

    Ensure-Directory -Path $TempDir
    Ensure-Directory -Path $TextureOutDir
    Ensure-Directory -Path $MetaOutDir

    $metaPath = Join-Path $MetaOutDir "font.buildmeta.json"
    $fontRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $FontPath
    $charsetRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $CharsetFile
    $fontHash = Get-FileSha256 -Path $FontPath
    $charsetHash = Get-FileSha256 -Path $CharsetFile
    $buildKey = Get-DerivedDataBuildKey -AssetType "Font" -BuildVersion $BuildVersion -Inputs @(
        "Configuration=$Configuration",
        "Platform=$Platform",
        "Variant=$VariantName",
        "FontPath=$fontRelativePath",
        "FontSha256=$fontHash",
        "CharsetPath=$charsetRelativePath",
        "CharsetSha256=$charsetHash",
        "FontSize=$fontSize",
        "AtlasWidth=$atlasWidth",
        "AtlasHeight=$atlasHeight"
    )

    $buildReason = Test-FontBuildRequired -MetaPath $metaPath -FontRelativePath $fontRelativePath `
        -CharsetRelativePath $charsetRelativePath -FontHash $fontHash -CharsetHash $charsetHash `
        -BuildKey $buildKey -BuildVersion $BuildVersion -Force $Force

    if ($null -eq $buildReason) {
        Write-Host "[BuildFonts] Skip: $VariantName"
        return "UpToDate"
    }

    Write-Host "[BuildFonts] Build : $VariantName"
    Write-Host "[BuildFonts] Reason: $buildReason"

    $ddcEntryDirectory = Get-DdcEntryDirectory -GeneratedRoot $generatedRoot -AssetType "Font" -BuildKey $buildKey
    if (-not $DisableDdc -and -not $Force) {
        $restore = Restore-FontDdc -DdcEntryDirectory $ddcEntryDirectory -TextureOutDir $TextureOutDir -MetaOutDir $MetaOutDir
        if ($restore.Hit) {
            $outputPaths = Get-FontOutputPaths -TextureOutDir $TextureOutDir -MetaOutDir $MetaOutDir | ForEach-Object {
                Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $_
            }
            if (@($outputPaths).Count -gt 0) {
                Write-FontBuildMeta -MetaPath $metaPath -VariantName $VariantName `
                    -FontRelativePath $fontRelativePath -CharsetRelativePath $charsetRelativePath `
                    -FontHash $fontHash -CharsetHash $charsetHash -BuildKey $buildKey `
                    -OutputPaths @($outputPaths) -BuildVersion $BuildVersion
                $script:ddcHitCount++
                $script:ddcRestoredBytes += [int64]$restore.Bytes
                Write-Host "[BuildFonts] DDC HIT: $VariantName"
                return "Restored"
            }
        }
        $script:ddcMissCount++
        Write-Host "[BuildFonts] DDC MISS: $VariantName"
    }

    # 専用出力フォルダ内の旧生成物を消し、廃止されたページが残らないようにする。
    Clear-FontOutputs -TextureOutDir $TextureOutDir -MetaOutDir $MetaOutDir
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

    Write-FontBuildMeta -MetaPath $metaPath -VariantName $VariantName `
        -FontRelativePath $fontRelativePath -CharsetRelativePath $charsetRelativePath `
        -FontHash $fontHash -CharsetHash $charsetHash -BuildKey $buildKey `
        -OutputPaths @($outputPaths) -BuildVersion $BuildVersion

    if (-not $DisableDdc) {
        $script:ddcWrittenBytes += Store-FontDdc -DdcEntryDirectory $ddcEntryDirectory `
            -TextureOutDir $TextureOutDir -MetaOutDir $MetaOutDir
    }
    return "Built"
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

$script:ddcHitCount = 0
$script:ddcMissCount = 0
$script:ddcRestoredBytes = [int64]0
$script:ddcWrittenBytes = [int64]0
$builtCount = 0
$restoredCount = 0
$upToDateCount = 0

$variants = @(
    [pscustomobject]@{ Name = "Latin"; Charset = $latinCharsetFile },
    [pscustomobject]@{ Name = "JP"; Charset = $jpCharsetFile }
)
foreach ($variant in $variants) {
    $result = Build-FontVariant -VariantName $variant.Name -FontPath $fontPath -CharsetFile $variant.Charset `
        -TempDir (Join-Path $tempRoot $variant.Name) -TextureOutDir (Join-Path $textureFontRoot $variant.Name) `
        -MetaOutDir (Join-Path $fontMetaRoot $variant.Name) -ConverterPath $fontConverterExe `
        -BuildVersion $BuildVersion -Force $Force -DisableDdc $DisableDdc
    switch ($result) {
        "Built" { $builtCount++ }
        "Restored" { $restoredCount++ }
        default { $upToDateCount++ }
    }
}

if (Test-Path -LiteralPath $tempRoot -PathType Container) {
    Remove-Item -LiteralPath $tempRoot -Force -Recurse -ErrorAction SilentlyContinue
}

Write-Host "[BuildFonts] Completed. Built=$builtCount Restored=$restoredCount UpToDate=$upToDateCount"
Write-Host "[BuildFonts] DDC Hits=$script:ddcHitCount Misses=$script:ddcMissCount Restored=$(Format-ByteSize $script:ddcRestoredBytes) Written=$(Format-ByteSize $script:ddcWrittenBytes) Disabled=$DisableDdc"
exit 0
