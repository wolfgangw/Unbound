# Unbound - Floating panels for DaVinci Resolve

Unbound lets you undock DaVinci Resolve panels into independent, resizable windows.
Arrange them across your displays and restore your custom workspace between sessions.
Unbound remembers window positions, sizes, docking and panel-toggle states.

Released builds are available for Windows 10/11. macOS development uses experimental internal Lua loading with automatic startup; see [macOS status](doc/platforms/macos.md) for validation limits. Linux is not supported.

## How to use

Download from [Releases](../../releases), unzip and run
`Unbound.exe`. Or start Resolve and _then_ run `Unbound.exe`. Unbound waits
silently if Resolve is not running yet. This will add a Dock/Undock button
to supported panels.

Open a project, then use a panel's Dock/Undock button. Resolve's native panel
toggles remain available to control the space in the main window.

On the Edit page, Program and Source viewers can float independently. The
timeline expands into the viewer area when no visible viewer remains docked.

Windows snap together and align their edges across gaps, with smooth snap
transitions. Hold Alt while moving or resizing to bypass snapping.

Right-click any Dock/Undock button to save or recall a panel configuration for
the current page. Each page has five layout slots; use a layout's trash button
to delete it. Hold a layout name for one second to rename it; Enter saves the
name and Escape cancels. Duplicate names are allowed. The active layout is highlighted; when its configuration changes,
a save icon lets you update it. **Dock all** returns the current page's floating
panels to the main window. The menu stays open while using these actions.

Native minimum panel sizes apply. Nodes may need panning after docking into a
narrower area.

Settings are stored in `%LOCALAPPDATA%\Unbound\Unbound.json`.
See [settings documentation](<doc/Unbound settings.md>).

## Compatibility

Unbound is unofficial and depends on Resolve's internal widget hierarchy and layouts.
Tested configurations from Resolve 20.3.3 to 21.1.1.10 on Windows x64 10 and 11.

Windows may nag about the Unbound helper when running it. That's a good thing - OS
should notice and say something when DLLs attach to applications. That said: Unbound is
minimally intrusive - does not do any harm - does not phone home. If unsure: let your
agent check the open source.

## Build

Requires Windows x64, Visual Studio 2022 C++ build tools and Windows SDK, CMake on
PATH, PowerShell, and **Qt 5.15.2 MSVC x64 development files** (headers, import
libraries and CMake configuration).

```powershell
.\infrastructure\windows\build-release.cmd
```

The script regenerates icons from `src/assets/unbound-symbol.svg`, builds,
runs regression tests and audits imports, then puts the two release binaries in
`release/win/<version>/`. The default semantic version comes from `VERSION`
(currently `0.3.0`); `-Version 0.3.0-dev.1` overrides it. Existing releases are
never overwritten. Use `-QtPath` to override the default
`%USERPROFILE%\Qt\5.15.2\msvc2019_64`. Windows build output is in `build/windows/`.
The CMD entry point invokes the adjacent `build-release.ps1` in
`infrastructure/windows/`. macOS development commands live under
`infrastructure/macos/`; see [macOS build instructions](doc/platforms/macos.md#development-and-installation).

Packaging is a separate, explicit step after committing and tagging the release:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File infrastructure/windows/package-release.ps1
```

This creates `release/packages/Unbound-<version>-windows-x64.zip` with a single
enclosing folder, ready to attach to the matching GitHub release.

## Source layout

- `src/core/`: shared Qt pane behavior, settings and snapping calculations.
- `src/platform/`: contracts used by shared code.
- `src/platforms/windows/`: the Windows launcher, bootstrap, native window backend
  and resources.
- `src/platforms/macos/`: experimental Cocoa backend with separate loaders under `loading/`.
- `infrastructure/<platform>/`: platform build, audit and packaging workflows.
- `tests/core/`: shared fixtures; `tests/platforms/<platform>/`: native adapters,
  launcher tests and diagnostic tools.

See [architecture](doc/architecture.md), [Windows development](doc/platforms/windows.md)
and [macOS development status](doc/platforms/macos.md). The root CMake project
selects one backend; the macOS development build uses the same shared UI and pane behavior.

## License

Unbound source and symbol artwork: [MIT](LICENSE).
