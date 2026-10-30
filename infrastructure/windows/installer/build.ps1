param(
    [Parameter(Mandatory=$true,ParameterSetName='Internal')][string]$InternalBuild,
    [Parameter(Mandatory=$true,ParameterSetName='Release')][string]$Version,
    [string]$OutputDirectory = ''
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
if($PSCmdlet.ParameterSetName -eq 'Internal') {
    if ($InternalBuild -notmatch '^\d{4}\.\d{2}\.\d{2}\.\d{3,}$') { throw 'Expected internal build identifier yyyy.mm.dd.NNN' }
    $runtimeBuild=$InternalBuild
} else {
    . (Join-Path $PSScriptRoot '../version.ps1')
    $Version = (Get-UnboundReleaseVersion -repoRoot $root -Version $Version).Version
    $runtimeBuild=$Version
}
$candidate=Join-Path $root "build/windows/unbound/$runtimeBuild"
$manifest=Get-Content -LiteralPath (Join-Path $candidate 'release-manifest.json') -Raw | ConvertFrom-Json
$payload=Join-Path $candidate 'Release'
foreach($file in $manifest.Files) {
    $source=Join-Path $payload ([IO.Path]::GetFileName($file.Path))
    if((Get-FileHash -LiteralPath $source).Hash -ne $file.Hash) { throw "Candidate hash mismatch: $source" }
}
$day=(Get-Date).ToString('yyyy.MM.dd')
$counter=1
$existing=@(Get-ChildItem (Join-Path $root 'build/windows/installer') -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match "^$([regex]::Escape($day))\.\d+$" })
foreach($item in $existing) { $counter=[Math]::Max($counter,1+[int]($item.Name.Split('.')[-1])) }
$packageBuild='{0}.{1:000}' -f $day,$counter
$destination=if($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $root "release/win-installer/$packageBuild" }
if(Test-Path -LiteralPath $destination) { throw "Installer destination already exists: $destination" }
$build=Join-Path $root "build/windows/installer/$packageBuild"
& cmake -S $PSScriptRoot -B $build -G 'Visual Studio 17 2022' -A x64 "-DUNBOUND_VERSION=$($manifest.Version)" "-DUNBOUND_BUILD=$($manifest.Revision)" "-DUNBOUND_PAYLOAD=$($payload.Replace('\','/'))"
if($LASTEXITCODE) { throw 'Installer configuration failed' }
& cmake --build $build --config Release -- /m:1 /nr:false
if($LASTEXITCODE) { throw 'Installer build failed' }
& (Join-Path $build 'Release/UnboundInstallerStateTests.exe')
if($LASTEXITCODE) { throw 'Installer running-state tests failed' }
& (Join-Path $build 'Release/UnboundInstallerTests.exe')
if($LASTEXITCODE) { throw 'Installer transaction tests failed' }
$info=(Get-Item -LiteralPath (Join-Path $build 'Release/UnboundInstaller.exe')).VersionInfo
if($info.FileVersion -ne $manifest.Version -or $info.ProductVersion -ne $manifest.Version -or
    $info.FilePrivatePart -ne $manifest.Revision) { throw 'Installer version resource mismatch' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$toolset=Get-ChildItem (Join-Path $vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$dumpbin=Join-Path $toolset.FullName 'bin/Hostx64/x64/dumpbin.exe'
foreach($binary in @((Join-Path $payload 'UnboundLuaLoader.dll'),(Join-Path $build 'Release/UnboundInstaller.exe'))) {
    $imports=& $dumpbin /dependents $binary
    if($LASTEXITCODE) { throw 'Dependency audit failed' }
    $allowed=if($binary.EndsWith('.dll')) { @('KERNEL32.dll') } else { @('KERNEL32.dll','USER32.dll','GDI32.dll','COMCTL32.dll','UxTheme.dll') }
    $dependencies=@($imports | ForEach-Object { if($_ -match '^\s+([\w.-]+\.dll)\s*$') { $Matches[1] } })
    if(!$dependencies.Count) { throw 'Missing dependency audit results' }
    foreach($dependency in $dependencies) { if($dependency -notin $allowed) { throw "Unexpected dependency: $dependency" } }
}
New-Item -ItemType Directory -Path $destination | Out-Null
$installerName=[IO.File]::ReadAllText((Join-Path $build 'installer-name.txt'))+'.exe'
Copy-Item -LiteralPath (Join-Path $build 'Release/UnboundInstaller.exe') -Destination (Join-Path $destination $installerName)
@{ RuntimeBuild=$runtimeBuild; Version=$manifest.Version; SourceHead=(& git -C $root rev-parse HEAD);
    Installer=(Get-FileHash (Join-Path $destination $installerName));
    Loader=(Get-FileHash (Join-Path $payload 'UnboundLuaLoader.dll'));
    Helper=(Get-FileHash (Join-Path $payload 'Unbound.dll')) } | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $build 'installer-manifest.json')
Write-Host "Installer ready: $(Join-Path $destination $installerName)"
