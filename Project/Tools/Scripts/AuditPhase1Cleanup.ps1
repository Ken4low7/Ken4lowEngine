param(
    [string]$ProjectFile = "Ken4lowEngine.vcxproj",
    [string]$OutputPath = "Generated/Phase1Audit.md"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = (Resolve-Path (Join-Path $scriptDir "../..")).Path
$projectPath = Join-Path $projectRoot $ProjectFile
$targetsPath = Join-Path $projectRoot "Directory.Build.targets"

if (-not (Test-Path -LiteralPath $projectPath))
{
    throw "Visual Studio project が見つかりません: $projectPath"
}

function Get-RelativeProjectPath
{
    param([string]$FullPath)

    # 監査結果はOSに依存しない表記へ統一する。
    return [System.IO.Path]::GetRelativePath($projectRoot, $FullPath).Replace([char]92, [char]47)
}

function Convert-ToLocalPath
{
    param([string]$Value)

    return $Value.Replace([char]92, [System.IO.Path]::DirectorySeparatorChar).Replace([char]47, [System.IO.Path]::DirectorySeparatorChar)
}

function Test-IsLiteralProjectPath
{
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value))
    {
        return $false
    }

    # MSBuild macro / Item metadata / wildcard は実ファイル参照として存在確認しない。
    if ($Value -match '\$\(' -or $Value -match '%\(' -or $Value.Contains('*') -or $Value.Contains('?'))
    {
        return $false
    }

    return $true
}

function Get-ProjectItems
{
    param(
        [string]$XmlPath,
        [string]$AttributeName
    )

    if (-not (Test-Path -LiteralPath $XmlPath))
    {
        return @()
    }

    [xml]$xml = Get-Content -LiteralPath $XmlPath -Raw -Encoding UTF8
    $nodes = $xml.SelectNodes("//*[local-name()='ClCompile' or local-name()='ClInclude']")
    $items = @()

    foreach ($node in $nodes)
    {
        $attribute = $node.Attributes[$AttributeName]
        if ($null -eq $attribute)
        {
            continue
        }

        $items += [PSCustomObject]@{
            Kind = $node.LocalName
            Path = $attribute.Value
        }
    }

    return $items
}

function Get-AdditionalIncludeDirectories
{
    param([string]$XmlPath)

    [xml]$xml = Get-Content -LiteralPath $XmlPath -Raw -Encoding UTF8
    $nodes = $xml.SelectNodes("//*[local-name()='AdditionalIncludeDirectories']")
    $directories = @()

    foreach ($node in $nodes)
    {
        foreach ($value in ($node.InnerText -split ';'))
        {
            $trimmed = $value.Trim()
            if ([string]::IsNullOrWhiteSpace($trimmed))
            {
                continue
            }

            $directories += $trimmed
        }
    }

    return @($directories | Sort-Object -Unique)
}

function Get-MissingProjectReferences
{
    param([object[]]$Items)

    $missing = @()
    foreach ($item in $Items)
    {
        if (-not (Test-IsLiteralProjectPath $item.Path))
        {
            continue
        }

        $normalized = Convert-ToLocalPath $item.Path
        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $normalized))
        if (-not (Test-Path -LiteralPath $fullPath))
        {
            $missing += $item
        }
    }

    return $missing
}

function Get-MissingIncludeDirectories
{
    param([string[]]$Directories)

    $missing = @()
    foreach ($directory in $Directories)
    {
        if (-not (Test-IsLiteralProjectPath $directory))
        {
            continue
        }

        $normalized = Convert-ToLocalPath $directory
        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $normalized))
        if (-not (Test-Path -LiteralPath $fullPath -PathType Container))
        {
            $missing += $directory
        }
    }

    return @($missing | Sort-Object -Unique)
}

function Get-Classification
{
    param([string]$RelativePath)

    # 分類処理は常にforward slashへ揃え、Windows / PowerShellの区切り文字差を吸収する。
    $normalized = $RelativePath.Replace([char]92, [char]47)

    if ($normalized -like 'Engine/Editor/*')
    {
        return 'Editor'
    }

    if ($normalized -like 'Engine/Scene/Actor/Character/*')
    {
        return 'Gameplay移行候補'
    }

    if ($normalized -like 'ApplicationLayer/*')
    {
        return 'Gameplay / Application'
    }

    if ($normalized -like 'Engine/*')
    {
        return 'Engine'
    }

    return 'Other'
}

$projectItems = Get-ProjectItems -XmlPath $projectPath -AttributeName "Include"
$missingReferences = Get-MissingProjectReferences -Items $projectItems
$removedItems = Get-ProjectItems -XmlPath $targetsPath -AttributeName "Remove"
$includeDirectories = Get-AdditionalIncludeDirectories -XmlPath $projectPath
$missingIncludeDirectories = Get-MissingIncludeDirectories -Directories $includeDirectories

$legacyPattern = '(?i)(FpsCamera|FPS|Player|Enemy|Boss|Bullet|Weapon|Crosshair|Reload|NoAmmo)'
$legacyProjectItems = @($projectItems | Where-Object { $_.Path -match $legacyPattern })
$legacyIncludeDirectories = @($includeDirectories | Where-Object { $_ -match $legacyPattern })

$sourceRoots = @(
    (Join-Path $projectRoot "Engine"),
    (Join-Path $projectRoot "ApplicationLayer")
) | Where-Object { Test-Path -LiteralPath $_ }

$sourceFiles = @()
foreach ($root in $sourceRoots)
{
    $sourceFiles += Get-ChildItem -LiteralPath $root -Recurse -File | Where-Object {
        $_.Extension -in '.h', '.hpp', '.cpp', '.cxx'
    }
}

$classification = @{}
foreach ($file in $sourceFiles)
{
    $relativePath = Get-RelativeProjectPath $file.FullName
    $category = Get-Classification $relativePath
    if (-not $classification.ContainsKey($category))
    {
        $classification[$category] = @()
    }

    $classification[$category] += $relativePath
}

$legacySourceFiles = @($sourceFiles | Where-Object {
    (Get-RelativeProjectPath $_.FullName) -match $legacyPattern
} | ForEach-Object {
    Get-RelativeProjectPath $_.FullName
})

$characterRoot = Join-Path $projectRoot "Engine/Scene/Actor/Character"
$characterFiles = @()
if (Test-Path -LiteralPath $characterRoot)
{
    $characterFiles = @(Get-ChildItem -LiteralPath $characterRoot -Recurse -File | ForEach-Object {
        Get-RelativeProjectPath $_.FullName
    })
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('# Phase 1 Cleanup Audit')
$lines.Add('')
$lines.Add("生成日時: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
$lines.Add('')
$lines.Add('## Visual Studio project の存在しない参照')
$lines.Add('')
if ($missingReferences.Count -eq 0)
{
    $lines.Add('- なし')
}
else
{
    foreach ($item in $missingReferences | Sort-Object Path -Unique)
    {
        $lines.Add("- [$($item.Kind)] ``$($item.Path)``")
    }
}

$lines.Add('')
$lines.Add('## 存在しない AdditionalIncludeDirectories')
$lines.Add('')
if ($missingIncludeDirectories.Count -eq 0)
{
    $lines.Add('- なし')
}
else
{
    foreach ($directory in $missingIncludeDirectories)
    {
        $lines.Add("- ``$directory``")
    }
}

$lines.Add('')
$lines.Add('## FPS / 旧ゲーム固有名称を含む AdditionalIncludeDirectories')
$lines.Add('')
if ($legacyIncludeDirectories.Count -eq 0)
{
    $lines.Add('- なし')
}
else
{
    foreach ($directory in $legacyIncludeDirectories | Sort-Object -Unique)
    {
        $lines.Add("- ``$directory``")
    }
}

$lines.Add('')
$lines.Add('## Directory.Build.targets の除外項目')
$lines.Add('')
if ($removedItems.Count -eq 0)
{
    $lines.Add('- なし')
}
else
{
    foreach ($item in $removedItems | Sort-Object Path -Unique)
    {
        $lines.Add("- [$($item.Kind)] ``$($item.Path)``")
    }
}

$lines.Add('')
$lines.Add('## FPS / 旧ゲーム固有名称を含む project 項目')
$lines.Add('')
if ($legacyProjectItems.Count -eq 0)
{
    $lines.Add('- なし')
}
else
{
    foreach ($item in $legacyProjectItems | Sort-Object Path -Unique)
    {
        $lines.Add("- [$($item.Kind)] ``$($item.Path)``")
    }
}

$lines.Add('')
$lines.Add('## 現存ソースの分類')
$lines.Add('')
foreach ($category in @('Engine', 'Gameplay移行候補', 'Gameplay / Application', 'Editor', 'Other'))
{
    $count = if ($classification.ContainsKey($category)) { $classification[$category].Count } else { 0 }
    $lines.Add("- **$category**: $count files")
}

$lines.Add('')
$lines.Add('## Engine/Scene/Actor/Character の現存ファイル')
$lines.Add('')
if ($characterFiles.Count -eq 0)
{
    $lines.Add('- なし')
}
else
{
    foreach ($path in $characterFiles | Sort-Object)
    {
        $lines.Add("- ``$path``")
    }
}

$lines.Add('')
$lines.Add('## 現存ソースで旧ゲーム固有名称を含むファイル')
$lines.Add('')
if ($legacySourceFiles.Count -eq 0)
{
    $lines.Add('- なし')
}
else
{
    foreach ($path in $legacySourceFiles | Sort-Object -Unique)
    {
        $lines.Add("- ``$path``")
    }
}

$lines.Add('')
$lines.Add('## 注意')
$lines.Add('')
$lines.Add('- 名前だけではLegacyと断定しない。削除前に参照箇所と移行先を確認する。')
$lines.Add('- ``$(...)`` のMSBuild macro、``%(...)`` のItem metadata、wildcardは存在しないファイルとして扱わない。')
$lines.Add('- Gameplay移行候補の物理的な移動はPhase 2以降で行う。')

$outputFullPath = if ([System.IO.Path]::IsPathRooted($OutputPath))
{
    $OutputPath
}
else
{
    Join-Path $projectRoot $OutputPath
}

$outputDirectory = Split-Path -Parent $outputFullPath
if (-not [string]::IsNullOrWhiteSpace($outputDirectory))
{
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

$lines | Set-Content -LiteralPath $outputFullPath -Encoding UTF8
Write-Host "Phase 1 audit written: $outputFullPath"
