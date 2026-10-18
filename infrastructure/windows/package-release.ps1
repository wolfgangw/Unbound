param([string]$Version = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $PSScriptRoot 'version.ps1')
$version = (Get-UnboundReleaseVersion -repoRoot $repoRoot -Version $Version).Version
$tag = "v$version"
$commit = & git -C $repoRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot read source commit.' }
$tagCommit = & git -C $repoRoot rev-parse --verify "$tag^{commit}"
if ($LASTEXITCODE -ne 0 -or $tagCommit -ne $commit) { throw "Tag $tag must point to HEAD before packaging." }
$changes = & git -C $repoRoot status --porcelain
if ($LASTEXITCODE -ne 0 -or $changes) { throw 'Commit source changes before packaging.' }
$release = Join-Path $repoRoot "release\win\$version"
$manifest = Get-Content -LiteralPath (Join-Path $repoRoot "build\windows\unbound\$version\release-manifest.json") -Raw | ConvertFrom-Json
if ($manifest.Version -ne $version) { throw 'Release manifest version mismatch.' }
$installerName = [IO.Path]::GetFileName($manifest.Installer.Path)
if (!$installerName -or [IO.Path]::GetExtension($installerName) -ne '.exe') { throw 'Missing installer in release manifest.' }
$installer = Join-Path $release $installerName
$info = (Get-Item -LiteralPath $installer).VersionInfo
if ($info.FileVersion -ne $version -or $info.ProductVersion -ne $version) { throw 'Embedded installer version mismatch.' }
if ((Get-FileHash -LiteralPath $installer).Hash -ne $manifest.Installer.Hash) { throw 'Installer hash mismatch.' }
$folder = "Unbound-$version-windows-x64"
$packages = Join-Path $repoRoot 'release\packages'
New-Item -ItemType Directory -Force -Path $packages | Out-Null
$destination = Join-Path $packages "$folder.zip"
if (Test-Path -LiteralPath $destination) { throw "Package already exists: $destination" }
$readme = @"
Unbound for DaVinci Resolve $version (Windows x64)

Close DaVinci Resolve, run $installerName and choose Install or Update.
Unbound starts automatically when you next open DaVinci Resolve.
Run the installer again to uninstall. Settings and layouts are always kept.

Settings: %LOCALAPPDATA%\Unbound\Unbound.json
Source: https://github.com/wolfgangw/unbound
Source tag: $tag
Source commit: $commit
Build ID: $($manifest.BuildId)
License: MIT (see LICENSE)
"@
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Open($destination, [IO.FileMode]::CreateNew)
$archive = $null
$complete = $false
try {
    $archive = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Create, $true)
    foreach ($name in @($installerName, 'LICENSE')) {
        $source = if ($name -eq 'LICENSE') { Join-Path $repoRoot $name } else { Join-Path $release $name }
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $source, "$folder/$name", [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
    $entry = $archive.CreateEntry("$folder/README.txt")
    $writer = New-Object IO.StreamWriter($entry.Open(), (New-Object Text.UTF8Encoding($false)))
    try { $writer.Write($readme.Replace("`r`n", "`n") + "`n") } finally { $writer.Dispose() }
    $archive.Dispose()
    $archive = $null
    $stream.Dispose()
    $complete = $true
} finally {
    if ($archive) { $archive.Dispose() }
    $stream.Dispose()
    if (!$complete) { Remove-Item -LiteralPath $destination }
}
Write-Host "Package ready: $destination"
Get-FileHash -LiteralPath $destination
