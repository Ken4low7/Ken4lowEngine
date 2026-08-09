param(
    [string]$ProjectDir = ".",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [switch]$Force,
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

    $buildReason = Test-MeshBuildRequired -OutputPath $finalOutputPath -MetaPath $metaPath `
        -SourceRelativePath $sourceRelativePath -OutputRelativePath $outputRelativePath `
        -DependencyFingerprint $dependencyFingerprint `
        -BuildVersion $BuildVersion -Force $Force

    if ($null -eq $buildReason) {
        Write-Host "[BuildMeshes] Skip: $relative"
        $upToDateCount++
        continue
    }

    Write-Host "[BuildMeshes] Convert: $relative"
    Write-Host "[BuildMeshes] Reason : $buildReason"

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
    $meta = [ordered]@{
        BuildVersion          = $BuildVersion
        AssetType             = "Mesh"
        SourcePath            = $sourceRelativePath
        OutputPath            = $outputRelativePath
        DependencyFingerprint = $dependencyFingerprint
        Dependencies          = $dependencyRecords
    }
    Write-BuildMeta -Meta $meta -MetaPath $metaPath
    $convertedCount++
}

Write-Host "[BuildMeshes] Completed. Converted=$convertedCount UpToDate=$upToDateCount Failed=$failedCount"
exit 0
