# Windows internal diagnostic builds

Normal build conventions and platform-independent design rules are in
[AGENTS.md](../../AGENTS.md#build-and-validate).

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
