# Native fixtures may enter Windows mouse capture/clip paths. Run them in a
# bounded child process, and restore desktop cursor freedom even on test failure.
function Invoke-UnboundNativeTest {
    param([string]$Program, [string[]]$Arguments)
    if (!('Unbound.TestInputRecovery' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Unbound {
    public static class TestInputRecovery {
        [DllImport("user32.dll")] public static extern bool ClipCursor(IntPtr rectangle);
    }
}
'@
    }
    $stdout = [IO.Path]::GetTempFileName()
    $stderr = [IO.Path]::GetTempFileName()
    $fixture = $null
    try {
        # Do not pass SW_HIDE: Windows applies it to the fixture's first native
        # test window, silently removing that window from snap/visibility tests.
        # Reuse our console rather than opening an additional console window.
        $launch = @{ FilePath=$Program; NoNewWindow=$true; PassThru=$true;
            RedirectStandardOutput=$stdout; RedirectStandardError=$stderr }
        if ($Arguments -and $Arguments.Count -gt 0) { $launch.ArgumentList=$Arguments }
        $fixture = Start-Process @launch
        # Retain the process handle: Windows PowerShell otherwise may lose the
        # exit code when a short-lived Start-Process child exits before waiting.
        $null = $fixture.Handle
        if (!$fixture.WaitForExit(30000)) {
            Stop-Process -Id $fixture.Id -ErrorAction SilentlyContinue
            throw "Native fixture exceeded 30 seconds: $Program $Arguments"
        }
        $fixture.WaitForExit()
        if ($fixture.ExitCode -ne 0) { throw "Native fixture failed with exit code $($fixture.ExitCode): $Program $Arguments" }
    } finally {
        if ($fixture -and !$fixture.HasExited) { Stop-Process -Id $fixture.Id -ErrorAction SilentlyContinue }
        [void][Unbound.TestInputRecovery]::ClipCursor([IntPtr]::Zero)
        foreach ($log in @($stdout, $stderr)) {
            if (Test-Path -LiteralPath $log) {
                Get-Content -LiteralPath $log
                Remove-Item -LiteralPath $log
            }
        }
    }
}
