param(
    [string]$ProjectDir = ".",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [int]$MipLevel = 0,
    [switch]$Force,
    [int]$BuildVersion = 3
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "BuildAssetCommon.ps1")

function Test-PixelArtTexture {
    param([string]$RelativePath)

    $normalized = $RelativePath.Replace('/', '\').ToLowerInvariant()
    $fileName = [System.IO.Path]::GetFileNameWithoutExtension($normalized)

    # 命名規約または PixelArt フォルダ配下を no-mip 対象として扱う。
    return $normalized.Contains('\pixelart\') -or
           $fileName.EndsWith("_pixel") -or
           $fileName.EndsWith("_nearest") -or
           $fileName.EndsWith("_nomip")
}

function Get-BuildMetaPath {
    param([string]$OutputPath)
    return ($OutputPath + ".buildmeta.json")
}

function Test-TextureBuildRequired {
    param(
        [System.IO.FileInfo]$SourceFile,
        [string]$OutputPath,
        [string]$MetaPath,
        [string]$SourceRelativePath,
        [string]$OutputRelativePath,
        [string]$SourceHash,
        [string]$Operation,
        [int]$MipLevel,
        [bool]$DisableMipMap,
        [int]$BuildVersion,
        [bool]$Force
    )

    if ($Force) { return "Force rebuild" }
    if (!(Test-Path -LiteralPath $OutputPath -PathType Leaf)) { return "Missing output" }
    if (!(Test-Path -LiteralPath $MetaPath -PathType Leaf)) { return "Missing metadata" }

    $meta = Read-BuildMeta -MetaPath $MetaPath
    if ($null -eq $meta) { return "Broken metadata" }
    if ([int]$meta.BuildVersion -ne $BuildVersion) { return "BuildVersion changed" }
    if ([string]$meta.AssetType -ne "Texture") { return "AssetType changed" }
    if ([string]$meta.Operation -ne $Operation) { return "Operation changed" }
    if ([string]$meta.SourcePath -ne $SourceRelativePath) { return "Source path changed" }
    if ([string]$meta.OutputPath -ne $OutputRelativePath) { return "Output path changed" }
    if ([string]$meta.SourceSha256 -ne $SourceHash) { return "Source content changed" }
    if ([int64]$meta.SourceSizeBytes -ne [int64]$SourceFile.Length) { return "Source size changed" }
    if ([int]$meta.MipLevel -ne $MipLevel) { return "MipLevel changed" }
    if ([bool]$meta.DisableMipMap -ne $DisableMipMap) { return "DisableMipMap changed" }

    return $null
}

function Write-TextureBuildMeta {
    param(
        [System.IO.FileInfo]$SourceFile,
        [string]$MetaPath,
        [string]$SourceRelativePath,
        [string]$OutputRelativePath,
        [string]$SourceHash,
        [string]$Operation,
        [int]$MipLevel,
        [bool]$DisableMipMap,
        [int]$BuildVersion
    )

    $meta = [ordered]@{
        BuildVersion     = $BuildVersion
        AssetType        = "Texture"
        Operation        = $Operation
        SourcePath       = $SourceRelativePath
        OutputPath       = $OutputRelativePath
        SourceSizeBytes  = [int64]$SourceFile.Length
        SourceSha256     = $SourceHash
        MipLevel         = $MipLevel
        DisableMipMap    = $DisableMipMap
    }

    Write-BuildMeta -Meta $meta -MetaPath $MetaPath
}

function Invoke-TextureConverter {
    param(
        [string]$ExePath,
        [string]$InputPath,
        [int]$MipLevel,
        [bool]$DisableMipMap = $false
    )

    $argList = @($InputPath)
    if ($MipLevel -ge 0 -and -not $DisableMipMap) {
        $argList += "-ml"
        $argList += "$MipLevel"
    }
    if ($DisableMipMap) {
        $argList += "-nomip"
    }

    Write-Host "[BuildTextures] EXE  : $ExePath"
    Write-Host "[BuildTextures] INPUT: $InputPath"
    Write-Host "[BuildTextures] ARGS : $($argList -join ' ')"

    $process = Start-Process -FilePath $ExePath -ArgumentList $argList -NoNewWindow -Wait -PassThru
    Write-Host "[BuildTextures] ExitCode: $($process.ExitCode)"
    return $process.ExitCode
}

Write-Host "[BuildTextures] ProjectDir    : $ProjectDir"
Write-Host "[BuildTextures] Configuration : $Configuration"
Write-Host "[BuildTextures] Platform      : $Platform"
Write-Host "[BuildTextures] MipLevel      : $MipLevel"
Write-Host "[BuildTextures] Force         : $Force"
Write-Host "[BuildTextures] BuildVersion  : $BuildVersion"

try {
    $ProjectDir = (Resolve-Path $ProjectDir).Path
}
catch {
    throw "ProjectDir not found: $ProjectDir"
}

$rootDir = [System.IO.Path]::GetFullPath((Join-Path $ProjectDir ".."))
$generatedRoot = Join-Path $rootDir "Generated"
$candidates = @(
    (Join-Path $generatedRoot "Bin\TextureConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Platform\$Configuration\TextureConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Configuration\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Platform\$Configuration\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Configuration\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\TextureConverter\TextureConverter.exe"),
    (Join-Path $ProjectDir "Tools\TextureConverter\$Platform\$Configuration\TextureConverter.exe")
)

$textureConverterExe = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $textureConverterExe) {
    throw "TextureConverter.exe not found. Candidates:`n$($candidates -join "`n")"
}

$textureSourceRoot = Join-Path $ProjectDir "Resources\Textures\Sources"
$textureCompiledRoot = Join-Path $ProjectDir "Resources\Textures\Compiled"
if (!(Test-Path -LiteralPath $textureSourceRoot -PathType Container)) {
    throw "Texture source root not found: $textureSourceRoot"
}
Ensure-Directory -Path $textureCompiledRoot

$convertExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr")
$copyExtensions = @(".dds")
$sourceFiles = Get-ChildItem -Path $textureSourceRoot -Recurse -File | Where-Object {
    ($convertExtensions -contains $_.Extension.ToLowerInvariant()) -or
    ($copyExtensions -contains $_.Extension.ToLowerInvariant())
}

$convertedCount = 0
$copiedCount = 0
$upToDateCount = 0
$failedCount = 0

foreach ($file in $sourceFiles) {
    $ext = $file.Extension.ToLowerInvariant()
    $relative = Get-RelativePathSafe -BasePath $textureSourceRoot -TargetPath $file.FullName
    $relativeWithoutExt = ([System.IO.Path]::ChangeExtension($relative, $null)).TrimEnd('.')
    $operation = if ($copyExtensions -contains $ext) { "Copy" } else { "Convert" }
    $finalOutputPath = if ($operation -eq "Copy") {
        Join-Path $textureCompiledRoot $relative
    }
    else {
        Join-Path $textureCompiledRoot ($relativeWithoutExt + ".dds")
    }

    Ensure-Directory -Path (Split-Path $finalOutputPath -Parent)
    $sourceRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $file.FullName
    $outputRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $finalOutputPath
    $sourceHash = Get-FileSha256 -Path $file.FullName
    $disableMipMap = if ($operation -eq "Convert") { Test-PixelArtTexture -RelativePath $relative } else { $false }
    $metaPath = Get-BuildMetaPath -OutputPath $finalOutputPath

    $buildReason = Test-TextureBuildRequired `
        -SourceFile $file `
        -OutputPath $finalOutputPath `
        -MetaPath $metaPath `
        -SourceRelativePath $sourceRelativePath `
        -OutputRelativePath $outputRelativePath `
        -SourceHash $sourceHash `
        -Operation $operation `
        -MipLevel $MipLevel `
        -DisableMipMap $disableMipMap `
        -BuildVersion $BuildVersion `
        -Force $Force

    if ($null -eq $buildReason) {
        Write-Host "[BuildTextures] Skip: $relative"
        $upToDateCount++
        continue
    }

    # 変数直後のコロンをドライブ修飾子として解釈されないよう、書式演算子で出力する。
    Write-Host ("[BuildTextures] {0}: {1}" -f $operation, $relative)
    Write-Host "[BuildTextures] Reason: $buildReason"

    if ($operation -eq "Copy") {
        Copy-Item -LiteralPath $file.FullName -Destination $finalOutputPath -Force
        Write-TextureBuildMeta -SourceFile $file -MetaPath $metaPath `
            -SourceRelativePath $sourceRelativePath -OutputRelativePath $outputRelativePath `
            -SourceHash $sourceHash -Operation $operation -MipLevel $MipLevel `
            -DisableMipMap $disableMipMap -BuildVersion $BuildVersion
        $copiedCount++
        continue
    }

    $sourceDir = Split-Path $file.FullName -Parent
    $sourceBaseName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    $generatedDdsPath = Join-Path $sourceDir ($sourceBaseName + ".dds")
    if (Test-Path -LiteralPath $generatedDdsPath) {
        Remove-Item -LiteralPath $generatedDdsPath -Force
    }

    $exitCode = Invoke-TextureConverter -ExePath $textureConverterExe -InputPath $file.FullName `
        -MipLevel $MipLevel -DisableMipMap $disableMipMap
    if ($exitCode -ne 0 -or !(Test-Path -LiteralPath $generatedDdsPath -PathType Leaf)) {
        Write-Warning "Texture conversion failed: $($file.FullName) ExitCode=$exitCode"
        $failedCount++
        continue
    }

    Move-Item -LiteralPath $generatedDdsPath -Destination $finalOutputPath -Force
    Write-TextureBuildMeta -SourceFile $file -MetaPath $metaPath `
        -SourceRelativePath $sourceRelativePath -OutputRelativePath $outputRelativePath `
        -SourceHash $sourceHash -Operation $operation -MipLevel $MipLevel `
        -DisableMipMap $disableMipMap -BuildVersion $BuildVersion
    $convertedCount++
}

Write-Host "[BuildTextures] Completed. Converted=$convertedCount CopiedDDS=$copiedCount UpToDate=$upToDateCount Failed=$failedCount"
exit 0