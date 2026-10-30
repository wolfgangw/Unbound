param(
    [string]$Version = '',
    [int]$BuildNumber = 0,
    [switch]$InternalBuild,
    [switch]$PaletteSizeDiagnostics,
    [switch]$LayoutTrace,
    [switch]$LayoutCoverExperiment,
    [string]$QtPath = "$env:USERPROFILE\Qt\5.15.2\msvc2019_64"
)
$ErrorActionPreference = 'Stop'
if ($LayoutTrace -and !$InternalBuild) { throw 'Layout tracing requires an internal build.' }
if ($LayoutCoverExperiment -and !$InternalBuild) { throw 'The layout cover experiment requires an internal build.' }
if ($PaletteSizeDiagnostics -and !$InternalBuild) { throw 'Palette size diagnostics require an internal build.' }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'version.ps1')
$releaseVersion = Get-UnboundReleaseVersion -repoRoot $repoRoot -Version $Version
$version = $releaseVersion.Version
$buildId = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
if ($BuildNumber -lt 0 -or $BuildNumber -gt 65535) { throw 'BuildNumber must be 0..65535 (the Windows numeric revision; default 0).' }
if (!(Test-Path -LiteralPath (Join-Path $QtPath 'lib\cmake\Qt5\Qt5Config.cmake'))) { throw "Qt 5.15.2 development files not found at $QtPath" }
$releaseRoot = Join-Path $repoRoot 'release\win'
New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
$mutex = New-Object Threading.Mutex($false, 'Local\UnboundWindowsReleaseBuild')
$locked = $false
. (Join-Path $PSScriptRoot 'generate-icons.ps1')
. (Join-Path $PSScriptRoot 'audit-dependencies.ps1')
. (Join-Path $PSScriptRoot 'test-input.ps1')
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
try {
    try { $locked = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $locked = $true }
    if (!$locked) { throw 'Another Unbound release build is running.' }
    $outputVersion = $version
    if ($InternalBuild) {
        $day = (Get-Date).ToString('yyyy.MM.dd')
        $counter = 1
        while (Test-Path -LiteralPath (Join-Path $releaseRoot ('{0}.{1:000}' -f $day, $counter))) { $counter++ }
        if ($counter -gt 65535) { throw 'Internal daily counter exceeds Windows revision limit.' }
        $outputVersion = '{0}.{1:000}' -f $day, $counter
        $BuildNumber = $counter
        $version = (($version -split '\+')[0]) + '+internal.' + ($day -replace '\.', '') + ".$counter"
    }
    $destination = Join-Path $releaseRoot $outputVersion
    if (Test-Path -LiteralPath $destination) { throw "Release already exists: $destination. Existing releases are never overwritten." }
    Build-UnboundIcons -repoRoot $repoRoot
    $build = Join-Path $repoRoot "build\windows\unbound\$outputVersion"
    Invoke-Checked 'cmake' @('-S', $repoRoot, '-B', $build,
        '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_PREFIX_PATH=$QtPath",
        "-DUNBOUND_VERSION=$version", "-DUNBOUND_BUILD=$BuildNumber", "-DUNBOUND_BUILD_ID=$buildId",
        "-DUNBOUND_PALETTE_SIZE_DIAGNOSTICS=$($PaletteSizeDiagnostics.IsPresent)",
        "-DUNBOUND_LAYOUT_TRACE=$($LayoutTrace.IsPresent)",
        "-DUNBOUND_LAYOUT_COVER_EXPERIMENT=$($LayoutCoverExperiment.IsPresent)")
    Invoke-Checked 'cmake' @('--build', $build, '--config', 'Release', '--', '/m:1', '/nr:false')
    & (Join-Path $repoRoot 'tests\platforms\windows\version\main.ps1')
    $loader = Join-Path $build 'Release\UnboundLuaLoader.dll'
    $dll = Join-Path $build 'Release\Unbound.dll'
    foreach ($file in @($loader, $dll)) {
        $info = (Get-Item -LiteralPath $file).VersionInfo
        if ($info.ProductVersion -ne $version -or $info.FileVersion -ne $version -or
            $info.FileMajorPart -ne $releaseVersion.Major -or $info.FileMinorPart -ne $releaseVersion.Minor -or
            $info.FileBuildPart -ne $releaseVersion.Patch -or $info.FilePrivatePart -ne $BuildNumber) {
            throw "Version resource mismatch: $file"
        }
    }
    $dependencyReport = Get-UnboundDependencies -loader $loader -dll $dll
    # Tests exercise the shared pane code and production Lua attachment entry.
    $testBuild = Join-Path $repoRoot 'build\windows\pane-tests'
    Invoke-Checked 'cmake' @('-S', (Join-Path $repoRoot 'tests\core\pane-float'), '-B', $testBuild,
        '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_PREFIX_PATH=$QtPath")
    Invoke-Checked 'cmake' @('--build', $testBuild, '--config', 'Release', '--', '/m:1', '/nr:false')
    $env:PATH = "$QtPath\bin;$env:PATH"
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:UNBOUND_LOG_FILE = Join-Path $build 'lua-fixture.log'
    try {
        Invoke-Checked (Join-Path $build 'Release\UnboundLuaLoaderTests.exe') @((Join-Path $build 'Release\UnboundLuaLoader.dll'))
        Invoke-Checked (Join-Path $build 'Release\UnboundLuaLoaderTests.exe') @((Join-Path $build 'Release\UnboundLuaLoader.dll'), '--existing')
        Invoke-Checked (Join-Path $build 'Release\UnboundInstallerTests.exe') @()
    } finally { Remove-Item Env:UNBOUND_LOG_FILE -ErrorAction SilentlyContinue }
    Invoke-Checked (Join-Path $build 'Release\UnboundViewerTests.exe') @()
    Invoke-Checked (Join-Path $build 'Release\UnboundCinemaTests.exe') @()
    Invoke-Checked (Join-Path $build 'Release\UnboundLayoutTests.exe') @()
    Invoke-Checked (Join-Path $build 'Release\UnboundLayoutTests.exe') @('--color-clips-startup')
    Invoke-Checked (Join-Path $build 'Release\UnboundLayoutTests.exe') @('--auxiliary-page-context')
    $test = Join-Path $testBuild 'Release\pane-float-smoke.exe'
    Invoke-Checked (Join-Path $testBuild 'Release\audition-activation-test.exe') @()
    Invoke-Checked $test @()
    Invoke-Checked $test @('--main-placement-test')
    Invoke-Checked $test @('--settings-json-test')
    Invoke-Checked $test @('--header-button-test')
    Invoke-Checked $test @('--eliding-label-test')
    Invoke-Checked $test @('--nodes-canvas-test')
    Invoke-Checked $test @('--shutdown-test')
    Invoke-Checked $test @('--close-all-test')
    Invoke-Checked $test @('--edit-sidebar-startup-test', $dll)
    Invoke-Checked $test @('--standalone-test', $dll)
    Invoke-Checked $test @('--standalone-test', '--visible-start', $dll)
    Invoke-Checked $test @('--standalone-test', '--toggle-state-test', $dll)
    Invoke-Checked $test @('--standalone-test', '--color-selected-test', $dll)
    Invoke-Checked $test @('--reclaim-test', $dll)
    Invoke-Checked $test @('--fairlight-test', $dll)
    Invoke-Checked $test @('--fairlight-test', '--restore', $dll)
    Invoke-Checked $test @('--discovery-test', $dll)
    $env:QT_QPA_PLATFORM = 'windows'
    try {
        if ($LayoutCoverExperiment) {
            Invoke-UnboundNativeTest (Join-Path $build 'Release\UnboundLayoutTests.exe') @('--layout-cover-only')
        }
        Invoke-UnboundNativeTest (Join-Path $build 'Release\UnboundLayoutTests.exe') @()
        Invoke-UnboundNativeTest (Join-Path $build 'Release\UnboundLayoutTests.exe') @('--index-header-drag')
        Invoke-UnboundNativeTest (Join-Path $build 'Release\UnboundLayoutTests.exe') @('--auxiliary-page-context')
        Invoke-UnboundNativeTest (Join-Path $build 'Release\UnboundLayoutTests.exe') @('--color-clips-startup')
        Invoke-UnboundNativeTest $test @('--audition-taskbar-native')
        Invoke-UnboundNativeTest $test @('--main-placement-test')
        Invoke-UnboundNativeTest $test @('--snap-native')
        Invoke-UnboundNativeTest $test @('--grid-native')
        Invoke-UnboundNativeTest $test @('--captionless-native')
        Invoke-UnboundNativeTest $test @('--frame-paint-native')
    }
    finally { $env:QT_QPA_PLATFORM = 'offscreen' }
    # Runtime files stay in build/; the only deliverable is the native installer.
    $hashes = @(Get-FileHash -LiteralPath $loader, $dll)
    $manifest = @{ Version=$version; BuildId=$buildId; Revision=$BuildNumber; BuiltAt=(Get-Date).ToString('o'); Qt=$QtPath; Files=$hashes; Dependencies=$dependencyReport }
    $manifest |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $build 'release-manifest.json')
    $installerArguments = @{ OutputDirectory=$destination }
    if ($InternalBuild) { $installerArguments.InternalBuild=$outputVersion }
    else { $installerArguments.Version=$version }
    & (Join-Path $PSScriptRoot 'installer/build.ps1') @installerArguments
    $installer = @(Get-ChildItem -LiteralPath $destination -Filter '*.exe')
    if ($installer.Count -ne 1) { throw 'Expected one installer deliverable.' }
    $manifest.Installer = Get-FileHash -LiteralPath $installer[0].FullName
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $build 'release-manifest.json')
    Write-Host "Unbound $version ready: $destination"
} finally {
    if ($locked) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
