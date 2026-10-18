function Get-UnboundDependencies {
    param([string]$exe, [string]$dll, [string]$loader)
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Cannot locate Visual Studio tools for dependency audit.' }
    $toolset = Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
    $dumpbin = Join-Path $toolset.FullName 'bin\Hostx64\x64\dumpbin.exe'
    $allowed = @('VERSION.dll','KERNEL32.dll','USER32.dll','SHELL32.dll','GDI32.dll','ole32.dll','dwmapi.dll','COMCTL32.dll',
        'Qt5Core.dll','Qt5Gui.dll','Qt5Widgets.dll','MSVCP140.dll','VCRUNTIME140.dll','VCRUNTIME140_1.dll')
    $dependencyReport = @{}
    foreach ($file in @($exe, $dll, $loader) | Where-Object { $_ }) {
        $report = & $dumpbin /dependents $file
        if ($LASTEXITCODE -ne 0) { throw "Dependency audit failed: $file" }
        $dependencies = @($report | ForEach-Object {
            if ($_ -match '^\s+([\w.-]+\.dll)\s*$') { $Matches[1] }
        })
        if (!$dependencies.Count) { throw "No dependencies found by audit: $file" }
        foreach ($dependency in $dependencies) {
            if ($dependency -notin $allowed -and $dependency -notmatch '^api-ms-win-crt-[\w-]+\.dll$') {
                throw "Unexpected runtime dependency $dependency in $file"
            }
            if ($file -eq $exe -and $dependency -notin @('VERSION.dll','KERNEL32.dll','USER32.dll')) {
                throw "Diagnostic executable must depend only on Windows: $dependency"
            }
            if ($file -eq $loader -and $dependency -ne 'KERNEL32.dll') {
                throw "Startup loader must depend only on KERNEL32.dll: $dependency"
            }
        }
        $dependencyReport[[IO.Path]::GetFileName($file)] = $dependencies
    }
    return $dependencyReport
}
