# Windows builds

Run the build from the repository root on Windows x64 with VS 2022 C++ tools,
the Windows SDK, CMake, PowerShell, Python 3 and Qt 5.15.2 MSVC x64 development
files. The default Qt directory is `%USERPROFILE%\Qt\5.15.2\msvc2019_64`.

```powershell
.\infrastructure\windows\build-release.cmd -InternalBuild
```

Use `-QtPath C:\Qt\5.15.2\msvc2019_64` for an alternate Qt installation,
or `-BuildJobs N` to change compilation parallelism (default up to four).
Fixtures run sequentially. Native-input fixtures require a desktop session.
The build uses `test-input.ps1` for process timeouts and cursor cleanup.

The layout-cover fixture allows one diagnosed limitation on Windows 10 builds
19041–21999. An independent probe must reproduce error 8 when excluding the
translucent menu. Cleanup, menu input/focus and display-affinity restoration
must still pass. The build prints `KNOWN LIMITATION` and records it in the
manifest's `ValidationLimitations`. Other failures, including on Windows 11,
remain fatal.

For an explicitly requested public build, omit `-InternalBuild`. The script
uses `VERSION` and stages `release/win/<version>/Unbound <version>.exe`.
Copy it to the [final release path](../../AGENTS.md#build-and-validate),
verify its embedded version and compare the final hash with the build's
`release-manifest.json`. Preserve existing artifacts. `-Version` overrides
the semantic version and `-BuildNumber` sets the numeric revision.
If a ZIP is requested, `infrastructure/windows/package-release.ps1` checks
clean tagged source, embedded versions and hashes, then packages the installer,
license and source revision under a single enclosing folder.

The [installer README](installer/README.md) covers installer-only rebuilding,
installation and recovery.

## Public Qt palette size measurements

```powershell
.\infrastructure\windows\build-release.cmd -InternalBuild -PaletteSizeDiagnostics
```

This opt-in internal build enables the shared CMake option
`UNBOUND_PALETTE_SIZE_DIAGNOSTICS` (off by default). Install with Resolve closed,
then start a fresh Resolve session. Open/restore floating Curves and Color Warper.
One deferred snapshot per palette attachment is written atomically under
`%TEMP%\Unbound-palette-sizes\`, named `curvesTab.json` or `colorMapperTab.json`.
Reattachment replaces that palette's snapshot. The same shared collector uses
Qt's temporary directory on macOS when the CMake option is enabled there.

Snapshots contain widget names/classes, parent indices, sizes, public Qt size
hints/policies, layout item requirements and selected stack-page indices. They
contain no control text, project data or addresses. Collection runs on the GUI
thread, does not change sizing, and is limited to 2,048 widgets and 2,048 items
per layout. It is absent from ordinary builds. Fixtures may produce snapshots
in the same temporary directory; use fresh Resolve captures for investigation.

For mode comparisons, select the desired Warper mode normally, then dock/refloat
and copy its snapshot before capturing another mode. Never switch modes through
private calls. Native update behavior and compact sizing require validation in
Resolve, beyond the normal Windows fixture suite.
