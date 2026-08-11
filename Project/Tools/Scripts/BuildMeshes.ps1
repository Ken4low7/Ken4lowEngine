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

function Get-MeshDependencyPaths {
    param([System.IO.FileInfo]$SourceFile)

    $paths = @($SourceFile.FullName)
    $sourceDir = $SourceFile.DirectoryName
    $extension = $SourceFile.Extension.ToLowerInvariant()

    if ($extension -eq ".gltf") {
        try {
            $gltf = Get-Content -LiteralPath $SourceFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
            foreach ($entry in @($gltf.buffers) + @($gltf.images)) {
                $uri = [string]$entry.uri
                if ([string]::IsNullOrWhiteSpace($uri) -or $uri.StartsWith("data:") -or [System.Uri]::IsWellFormedUriString($uri, [System.UriKind]::Absolute)) {
                    continue
                }

                $dependencyPath = [System.IO.Path]::GetFullPath((Join-Path $sourceDir ([System.Uri]::UnescapeDataString($uri))))
                if (Test-Path -LiteralPath $dependencyPath -PathType Leaf) {
                    $paths += $dependencyPath
                }
            }
        }
        catch {
            Write-Warning "Failed to read glTF dependencies: $($SourceFile.FullName)"
        }
    }
    elseif ($extension -eq ".obj") {
        $mtlPaths = @()
        foreach ($line in Get-Content -LiteralPath $SourceFile.FullName -ErrorAction SilentlyContinue) {
            if ($line -match '^\s*mtllib\s+(.+?)\s*$') {
                $mtlPath = [System.IO.Path]::GetFullPath((Join-Path $sourceDir $Matches[1]))
                if (Test-Path -LiteralPath $mtlPath -PathType Leaf) {
                    $paths += $mtlPath
                    $mtlPaths += $mtlPath
                }
            }
        }

        foreach ($mtlPath in $mtlPaths) {
            $mtlDir = Split-Path $mtlPath -Parent
            foreach ($line in Get-Content -LiteralPath $mtlPath -ErrorAction SilentlyContinue) {
                if ($line -match '^\s*(map_Ka|map_Kd|map_Ks|map_Ns|map_d|map_bump|bump|disp|decal|norm)\s+(.+?)\s*$') {
                    $argument = $Matches[2]
                    $textureReference = if ($argument -match '"([^"]+)"\s*$') {
                        $Matches[1]
                    }
                    else {
                        ($argument -split '\s+')[-1]
                    }

                    $texturePath = [System.IO.Path]::GetFullPath((Join-Path $mtlDir $textureReference))
                    if (Test-Path -LiteralPath $texturePath -PathType Leaf) {
                        $paths += $texturePath
                    }
                }
            }
        }
    }

    # 外部依存ファイルも含めた指紋を作り、参照データの更新漏れを防ぐ。
    return $paths | Sort-Object -Unique
}

function Test-MeshBuildRequired {
    param(
        [string]$OutputPath,
        [string]$MetaPath,
        [string]$SourceRelativePath,
        [string]$OutputRelativePath,
        [string]$DependencyFingerprint,
        [string]$BuildKey,
        [int]$BuildVersion,
        [bool]$Force
    )

    if ($Force) { return "Force rebuild" }
    if (!(Test-Path -LiteralPath $OutputPath -PathType Leaf)) { return "Missing output" }
    if (!(Test-Path -LiteralPath $MetaPath -PathType Leaf)) { return "Missing metadata" }

    $meta = Read-BuildMeta -MetaPath $MetaPath
    if ($null -eq $meta) { return "Broken metadata" }
    if ([int]$meta.BuildVersion -ne $BuildVersion) { return "BuildVersion changed" }
    if ([string]$meta.AssetType -ne "Mesh") { return "AssetType changed" }
    if ([string]$meta.SourcePath -ne $SourceRelativePath) { return "Source path changed" }
    if ([string]$meta.OutputPath -ne $OutputRelativePath) { return "Output path changed" }
    if ([string]$meta.DependencyFingerprint -ne $DependencyFingerprint) { return "Source or dependency changed" }
    if ([string]$meta.BuildKey -ne $BuildKey) { return "BuildKey changed" }
    return $null
}

function Invoke-MeshConverter {
    param(
        [string]$ExePath,
        [string]$InputPath
    )

    $process = Start-Process -FilePath $ExePath -ArgumentList @($InputPath) -NoNewWindow -Wait -PassThru
    Write-Host "[BuildMeshes] ExitCode: $($process.ExitCode)"
    return $process.ExitCode
}

function Write-MeshBuildMeta {
    param(
        [string]$MetaPath,
        [string]$SourceRelativePath,
        [string]$OutputRelativePath,
        [string]$DependencyFingerprint,
        [object[]]$DependencyRecords,
        [string]$BuildKey,
        [int]$BuildVersion
    )

    $meta = [ordered]@{
        BuildVersion          = $BuildVersion
        AssetType             = "Mesh"
        BuildKey              = $BuildKey
        SourcePath            = $SourceRelativePath
        OutputPath            = $OutputRelativePath
        DependencyFingerprint = $DependencyFingerprint
        Dependencies          = $DependencyRecords
    }
    Write-BuildMeta -Meta $meta -MetaPath $MetaPath
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
    (Join-Path $generatedRoot "Bin\MeshConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Platform\$Configuration\MeshConverter.exe"),
    (Join-Path $generatedRoot "Bin\$Configuration\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Platform\$Configuration\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\Bin\$Configuration\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\MeshConverter\MeshConverter.exe"),
    (Join-Path $ProjectDir "Tools\MeshConverter\$Platform\$Configuration\MeshConverter.exe")
)

$meshConverterExe = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $meshConverterExe) {
    throw "MeshConverter.exe not found. Candidates:`n$($candidates -join "`n")"
}

$modelSourceRoot = Join-Path $ProjectDir "Resources\Models\Sources"
$modelCompiledRoot = Join-Path $ProjectDir "Resources\Models\Compiled"
if (!(Test-Path -LiteralPath $modelSourceRoot -PathType Container)) {
    throw "Model source root not found: $modelSourceRoot"
}
Ensure-Directory -Path $modelCompiledRoot

$extensions = @(".gltf", ".glb", ".obj")
$sourceFiles = Get-ChildItem -Path $modelSourceRoot -Recurse -File | Where-Object {
    $extensions -contains $_.Extension.ToLowerInvariant()
}

$convertedCount = 0
$upToDateCount = 0
$failedCount = 0
$ddcHitCount = 0
$ddcMissCount = 0
$ddcRestoredBytes = [int64]0
$ddcWrittenBytes = [int64]0

foreach ($file in $sourceFiles) {
    $relative = Get-RelativePathSafe -BasePath $modelSourceRoot -TargetPath $file.FullName
    $relativeWithoutExt = ([System.IO.Path]::ChangeExtension($relative, $null)).TrimEnd('.')
    $finalOutputPath = Join-Path $modelCompiledRoot ($relativeWithoutExt + ".kmesh")
    $metaPath = $finalOutputPath + ".buildmeta.json"
    Ensure-Directory -Path (Split-Path $finalOutputPath -Parent)

    $dependencyPaths = Get-MeshDependencyPaths -SourceFile $file
    $dependencyRecords = Get-DependencyRecords -ProjectDir $ProjectDir -Paths $dependencyPaths
    $dependencyFingerprint = Get-DependencyFingerprint -Records $dependencyRecords
    $sourceRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $file.FullName
    $outputRelativePath = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $finalOutputPath
    $buildKey = Get-DerivedDataBuildKey -AssetType "Mesh" -BuildVersion $BuildVersion -Inputs @(
        "Configuration=$Configuration",
        "Platform=$Platform",
        "SourcePath=$sourceRelativePath",
        "DependencyFingerprint=$dependencyFingerprint"
    )

    $buildReason = Test-MeshBuildRequired -OutputPath $finalOutputPath -MetaPath $metaPath `
        -SourceRelativePath $sourceRelativePath -OutputRelativePath $outputRelativePath `
        -DependencyFingerprint $dependencyFingerprint -BuildKey $buildKey `
        -BuildVersion $BuildVersion -Force $Force

    if ($null -eq $buildReason) {
        Write-Host "[BuildMeshes] Skip: $relative"
        $upToDateCount++
        continue
    }

    Write-Host "[BuildMeshes] Convert: $relative"
    Write-Host "[BuildMeshes] Reason : $buildReason"

    $ddcEntryDirectory = Get-DdcEntryDirectory -GeneratedRoot $generatedRoot -AssetType "Mesh" -BuildKey $buildKey
    $ddcPayloadPath = Join-Path $ddcEntryDirectory "payload.kmesh"
    if (-not $DisableDdc -and -not $Force) {
        $restore = Restore-DdcFile -CachePath $ddcPayloadPath -OutputPath $finalOutputPath
        if ($restore.Hit) {
            Write-Host "[BuildMeshes] DDC HIT: $relative"
            Write-MeshBuildMeta -MetaPath $metaPath -SourceRelativePath $sourceRelativePath `
                -OutputRelativePath $outputRelativePath -DependencyFingerprint $dependencyFingerprint `
                -DependencyRecords $dependencyRecords -BuildKey $buildKey -BuildVersion $BuildVersion
            $ddcHitCount++
            $ddcRestoredBytes += [int64]$restore.Bytes
            continue
        }
        $ddcMissCount++
        Write-Host "[BuildMeshes] DDC MISS: $relative"
    }

    $sourceDir = Split-Path $file.FullName -Parent
    $sourceBaseName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    $generatedKmeshPath = Join-Path $sourceDir ($sourceBaseName + ".kmesh")
    if (Test-Path -LiteralPath $generatedKmeshPath) {
        Remove-Item -LiteralPath $generatedKmeshPath -Force
    }

    $exitCode = Invoke-MeshConverter -ExePath $meshConverterExe -InputPath $file.FullName
    if ($exitCode -ne 0 -or !(Test-Path -LiteralPath $generatedKmeshPath -PathType Leaf)) {
        Write-Warning "Mesh conversion failed: $($file.FullName) ExitCode=$exitCode"
        $failedCount++
        continue
    }

    Move-Item -LiteralPath $generatedKmeshPath -Destination $finalOutputPath -Force
    Write-MeshBuildMeta -MetaPath $metaPath -SourceRelativePath $sourceRelativePath `
        -OutputRelativePath $outputRelativePath -DependencyFingerprint $dependencyFingerprint `
        -DependencyRecords $dependencyRecords -BuildKey $buildKey -BuildVersion $BuildVersion

    if (-not $DisableDdc) {
        $ddcWrittenBytes += Store-DdcFile -SourcePath $finalOutputPath -CachePath $ddcPayloadPath
    }
    $convertedCount++
}

Write-Host "[BuildMeshes] Completed. Converted=$convertedCount UpToDate=$upToDateCount Failed=$failedCount"
Write-Host "[BuildMeshes] DDC Hits=$ddcHitCount Misses=$ddcMissCount Restored=$(Format-ByteSize $ddcRestoredBytes) Written=$(Format-ByteSize $ddcWrittenBytes) Disabled=$DisableDdc"
exit $(if ($failedCount -gt 0) { 1 } else { 0 })
