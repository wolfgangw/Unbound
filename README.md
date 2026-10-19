# Unbound - Floating panels for DaVinci Resolve

Unbound lets you undock DaVinci Resolve panels into independent, resizable windows.
Arrange them across your displays and restore your custom workspace between sessions.
Unbound remembers window positions, sizes, docking and panel-toggle states.

Unbound supports Windows 10/11 (x64) and macOS. The universal Mac app includes Apple
Silicon and Intel code. No Linux port yet.

## How to use

Download from [Releases](../../releases) and run the Unbound installer.
Choose **Install**, then open DaVinci Resolve.
Unbound starts automatically and adds a Dock/Undock button to supported panels.
Run the installer again to update or uninstall. Uninstall always keeps your
settings and layouts.

Open a project, then use a panel's Dock/Undock button. Resolve's native panel
toggles remain available to control the space in the main window.

On the Edit page, Program and Source viewers can float independently. The
timeline expands into the viewer area when no visible viewer remains docked.

Windows snap together and align their edges across gaps. Hold Alt while moving
or resizing to bypass snapping.

Right-click any Dock/Undock button to save or recall layouts for
the current page. Each page has five layout slots; use a layout's trash button
to delete it. Hold a layout name for one second to rename it; Enter saves the
name and Escape cancels. Duplicate names are allowed. The active layout is highlighted;
when its configuration changes, a save icon lets you update it. **Dock all** returns
the current page's floating panels to the main window.

Native minimum panel sizes apply. Nodes may need Cleanup after docking into a
narrower area.

Settings are stored in `%LOCALAPPDATA%\Unbound\Unbound.json` on Windows and
`~/Library/Application Support/Unbound/Unbound.json` on macOS.
See [settings documentation](<doc/Unbound settings.md>).

## Compatibility

Unbound is unofficial and depends on Resolve's internal widget hierarchy and layouts.
Tested configurations range from Resolve 20.3.3 to 21.1.1.10 on Windows 10/11,
and include Resolve Free 21.1.1.10 on an Apple M2 with macOS 27.0.1.

**Intel Mac support is untested.** The universal build includes Intel code, but
we do not have an Intel Mac to validate it. It is offered for users willing to
try it and report their results, including their Mac model, macOS and Resolve versions,
and Unbound version. Apple Silicon remains the tested Mac configuration.

On Windows and macOS, Unbound starts automatically through DaVinci Resolve's Lua
startup mechanism. The installer places a small startup script in Resolve's
per-user scripts folder. That script loads Unbound's native helper, which uses
Resolve's existing Qt libraries and initializes on its UI thread. No Resolve
application files are modified, and no background service, network contact or
separate launcher is needed.

Code signing and notarization are still on the paperwork pile: Windows builds
are unsigned, and macOS builds are ad-hoc signed rather than Developer ID signed
and notarized. Please indulge this small project's missing official rubber stamps.
Thankfully, our operating systems still offer ways to decide for ourselves what
to run. For Unbound downloaded from this repository, please use the per-app
exception when offered:

- **Windows:** in the SmartScreen warning, choose **More info**, then **Run anyway**.
  Availability depends on your system's security policy; see
  [Microsoft's SmartScreen guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation).
- **macOS:** try opening the app once, then go to **System Settings → Privacy &
  Security → Open Anyway** and confirm. See
  [Apple's instructions](https://support.apple.com/en-us/102445).

In this day and age, of course, just let your agent check code and application files.

## Build

Requires Windows x64, Visual Studio 2022 C++ build tools and Windows SDK, CMake on
PATH, PowerShell, Python 3, and **Qt 5.15.2 MSVC x64 development files** (headers, import
libraries and CMake configuration).

```powershell
.\infrastructure\windows\build-release.cmd
```

The script regenerates icons from `src/assets/unbound-symbol.svg`, builds,
runs regression tests and audits imports, then puts the self-contained installer in
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
- `src/platforms/windows/`: the Windows startup loader, installer, native window backend
  and resources.
- `src/platforms/macos/`: Cocoa backend, startup loader and native installer.
- `infrastructure/<platform>/`: platform build, audit and packaging workflows.
- `tests/core/`: shared fixtures; `tests/platforms/<platform>/`: native adapters,
  loader/installer tests and diagnostic tools.

See [architecture](doc/architecture.md), [Windows development](doc/platforms/windows.md)
and [macOS development](doc/platforms/macos.md). The root CMake project
selects one backend; both platforms use the same shared UI and pane behavior.

## License

Unbound source and symbol artwork: [MIT](LICENSE).
