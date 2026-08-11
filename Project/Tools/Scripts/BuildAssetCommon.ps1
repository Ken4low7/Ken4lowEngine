$ErrorActionPreference = "Stop"

function Ensure-Directory {
    param([string]$Path)

    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Get-RelativePathSafe {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseFull = [System.IO.Path]::GetFullPath($BasePath)
    $targetFull = [System.IO.Path]::GetFullPath($TargetPath)
    $baseUri = New-Object System.Uri(($baseFull.TrimEnd('\') + '\'))
    $targetUri = New-Object System.Uri($targetFull)
    $relativeUri = $baseUri.MakeRelativeUri($targetUri)
    $relativePath = [System.Uri]::UnescapeDataString($relativeUri.ToString())
    return $relativePath.Replace('/', '\')
}

function Get-ProjectRelativePath {
    param(
        [string]$ProjectDir,
        [string]$TargetPath
    )

    # JSON には端末固有の絶対パスではなく、ProjectDir 基準の相対パスを保存する。
    return (Get-RelativePathSafe -BasePath $ProjectDir -TargetPath $TargetPath).Replace('\', '/')
}

function Get-FileSha256 {
    param([string]$Path)

    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-StringSha256 {
    param([string]$Text)

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        $hash = $sha256.ComputeHash($bytes)
        return ([System.BitConverter]::ToString($hash)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
}

function Get-DerivedDataBuildKey {
    param(
        [string]$AssetType,
        [int]$BuildVersion,
        [string[]]$Inputs
    )

    $lines = @("AssetType=$AssetType", "BuildVersion=$BuildVersion")
    $lines += @($Inputs)
    return Get-StringSha256 -Text ($lines -join "`n")
}

function Get-DdcEntryDirectory {
    param(
        [string]$GeneratedRoot,
        [string]$AssetType,
        [string]$BuildKey
    )

    if ([string]::IsNullOrWhiteSpace($BuildKey) -or $BuildKey.Length -lt 2) {
        throw "Invalid DDC BuildKey."
    }

    $safeType = ($AssetType -replace '[^A-Za-z0-9_.-]', '_')
    return Join-Path $GeneratedRoot ("DerivedDataCache\{0}\{1}\{2}" -f $safeType, $BuildKey.Substring(0, 2), $BuildKey)
}

function Restore-DdcFile {
    param(
        [string]$CachePath,
        [string]$OutputPath
    )

    if (!(Test-Path -LiteralPath $CachePath -PathType Leaf)) {
        return [pscustomobject]@{ Hit = $false; Bytes = [int64]0 }
    }

    Ensure-Directory -Path (Split-Path $OutputPath -Parent)
    Copy-Item -LiteralPath $CachePath -Destination $OutputPath -Force
    $bytes = [int64](Get-Item -LiteralPath $CachePath).Length
    return [pscustomobject]@{ Hit = $true; Bytes = $bytes }
}

function Store-DdcFile {
    param(
        [string]$SourcePath,
        [string]$CachePath
    )

    if (!(Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        return [int64]0
    }

    Ensure-Directory -Path (Split-Path $CachePath -Parent)
    $tempPath = $CachePath + ".tmp"
    Copy-Item -LiteralPath $SourcePath -Destination $tempPath -Force
    Move-Item -LiteralPath $tempPath -Destination $CachePath -Force
    return [int64](Get-Item -LiteralPath $SourcePath).Length
}

function Get-DirectoryFileBytes {
    param([string]$Path)

    if (!(Test-Path -LiteralPath $Path -PathType Container)) {
        return [int64]0
    }

    $total = [int64]0
    Get-ChildItem -LiteralPath $Path -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
        $total += [int64]$_.Length
    }
    return $total
}

function Format-ByteSize {
    param([int64]$Bytes)

    if ($Bytes -ge 1GB) { return ("{0:N2} GiB" -f ($Bytes / 1GB)) }
    if ($Bytes -ge 1MB) { return ("{0:N2} MiB" -f ($Bytes / 1MB)) }
    if ($Bytes -ge 1KB) { return ("{0:N2} KiB" -f ($Bytes / 1KB)) }
    return "$Bytes B"
}

function Read-BuildMeta {
    param([string]$MetaPath)

    try {
        return Get-Content -LiteralPath $MetaPath -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    catch {
        return $null
    }
}

function Write-BuildMeta {
    param(
        [object]$Meta,
        [string]$MetaPath
    )

    Ensure-Directory -Path (Split-Path $MetaPath -Parent)
    $Meta | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $MetaPath -Encoding UTF8
}

function Get-DependencyRecords {
    param(
        [string]$ProjectDir,
        [string[]]$Paths
    )

    $records = @()
    foreach ($path in ($Paths | Where-Object { $_ } | Sort-Object -Unique)) {
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
            continue
        }

        $file = Get-Item -LiteralPath $path
        $records += [ordered]@{
            Path      = Get-ProjectRelativePath -ProjectDir $ProjectDir -TargetPath $file.FullName
            SizeBytes = [int64]$file.Length
            Sha256    = Get-FileSha256 -Path $file.FullName
        }
    }

    return $records
}

function Get-DependencyFingerprint {
    param([object[]]$Records)

    $text = ($Records | ForEach-Object {
        "{0}|{1}|{2}" -f $_.Path, $_.SizeBytes, $_.Sha256
    }) -join "`n"

    return Get-StringSha256 -Text $text
}
