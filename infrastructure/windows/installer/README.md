# Windows installer

`infrastructure/windows/build-release.cmd -InternalBuild` builds and tests the
runtime and its self-contained native installer in `release/win/<build>/`.
To rebuild only the installer UI around an existing tested runtime:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File infrastructure/windows/installer/build.ps1 -InternalBuild yyyy.mm.dd.NNN
```

These UI-only rebuilds go under `release/win-installer/<packaging-date.counter>/`.
Its layout and wording come from the [shared installer definition](../../installer/README.md),
also used by the macOS app. Versions are shown on the second line inside each
action button; internal builds use `yyyy.mm.dd.NNN`. Status/operation feedback
is at the bottom below a divider.
Building the installer requires Python 3 to generate the shared header; the app
has no Python or JSON runtime dependency.
Internal filenames and window titles are `Unbound <version> (<runtime-build>)`,
for example `Unbound 0.3.0 (2026.10.18.003).exe`. Release installers omit the
build suffix: `Unbound 0.3.0.exe`. Use `-Version 0.3.0` instead of `-InternalBuild`
to package an explicitly prepared release candidate. The MIT notice remains
embedded in the executable without a separate License button.
Packaging has its own daily counter; the window identifies the bundled runtime.
It embeds the helper, Qt-free Lua loader, startup hook and exact candidate version.
No ZIP, tag or public release is created. Runtime DLLs remain in `build/windows/`;
the installer embeds them and is the only Windows deliverable.

Install/Update and Uninstall operate on the current account and require Resolve
closed; neither terminates it. Settings and layouts are always retained.
There is no settings-deletion action; Uninstall is disabled when Unbound is absent.
Running-state monitoring sleeps between 500 ms checks on a dedicated Windows
background-mode thread (idle CPU priority fallback). While Resolve is running,
it checks a cached synchronization handle instead of enumerating processes.
Discovery scans occur only without a usable live handle and stop at the first
match. Only state changes post a UI notification; stable checks read no installed
files or settings and request no repaint. Closing the installer stops the worker.
The private runtime lives under `%LOCALAPPDATA%/Unbound/Runtime`. Only
`Unbound.scriptlib` goes into `%APPDATA%/Blackmagic Design/DaVinci Resolve/Support/Fusion/Scripts`.
No service, scheduled task, registry startup entry or visible Scripts menu item
is installed. The installer uses Windows APIs directly and needs no Qt runtime
or development toolchain. Current installers are unsigned; see the
[README](../../../README.md#compatibility) for the per-app SmartScreen exception.

The startup hook uses wide-character Windows APIs to support non-ASCII account
paths. It loads the Qt-free adapter once and returns. The adapter validates the
loaded Qt 5.15.2 application before loading the helper. Attachment and duplicate
controller checks happen on the Qt GUI thread. The log is
`%LOCALAPPDATA%/Unbound/lua-bootstrap.log`; `UNBOUND_LOG_FILE` isolates fixture logs.

The build-only `UnboundInstallTool` invokes the same transaction implementation
for development installation. It takes the runtime directory, hook file and
version file as arguments. It is not included in the distributable installer.
