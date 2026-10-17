# Compatibility entry point; Windows implementation lives under infrastructure/.
param(
    [string]$Version = '',
    [int]$BuildNumber = 0,
    [switch]$InternalBuild,
    [string]$QtPath = "$env:USERPROFILE\Qt\5.15.2\msvc2019_64"
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'infrastructure\windows\build-release.ps1') @PSBoundParameters
