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

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($text)
        $hash = $sha256.ComputeHash($bytes)
        return ([System.BitConverter]::ToString($hash)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
}
