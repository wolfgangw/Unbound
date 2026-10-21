# Cinema Viewer keyboard routing

Windows Resolve's `UiCinemaViewerDialog` reparents the selected image widget and
hides its original top-level window. After Unbound undocking that window is the
floating host. Resolve's main window remains visible, so its `UiMainMenuBar`
application shortcuts remain eligible alongside Cinema Viewer's copies.
The failing trace activates the menu's `QShortcut`; the working docked trace
activates Cinema's corresponding shortcut. Native and Qt focus are correct in
both cases. Merely refocusing Cinema does not repair this.

`CinemaViewerVisibility` matches native docked behavior by hiding the remaining
main window while Cinema contains an undocked Edit image widget. It restores
only visibility it changed, after native Cinema cleanup, without activating the
main window over another app. It does not move viewer content, change shortcuts,
forward keys or poll. This workaround is Windows-only; macOS does not reproduce
the defect. Geometry, docking and saved layouts remain owned by the existing
shared code.

The shared geometry adapter must distinguish visibility from geometry changes:
inherited Hide/Show events arrive while regions may still have Unbound's
collapsed/expanded geometry. Those events update visibility only; treating them
as native geometry would overwrite the docking baseline with a zero-height
viewer region. `UnboundViewerTests` covers repeated main-window hide/show followed
by docking in Single and Dual Viewer configurations, without a resize to repair
the layout.

`UnboundCinemaTests` runs with the regular Windows build. It covers Program and
Source, repeated entry/exit, cancellation, rapid reentry, native dialog
destruction, adapter detachment, already-hidden main windows, unaffected docked
viewers/other dialogs, and geometry/shortcut preservation. Live Resolve testing
still needs a fresh helper process: single Program floating, both floating in
Dual Viewer Mode, and a docked active Source. Check arrows, J/K/L, Ctrl+F/Escape,
then normal shortcuts after exit and switching away/back to Resolve.

For a bounded passive trace, configure this directory with Qt 5.15.2 and build
`cinema-focus-probe.exe` plus `CinemaFocusProbe.dll`. Run the EXE with Resolve
open. It records public Qt/Win32 focus, shortcut activation and widget metadata
beside the DLL for 120 seconds (up to 1,500 events), then removes its filters.
Running it again during capture writes a snapshot; after expiry it starts a new
capture. No custom property getters, private handlers or `dumpObjectInfo` calls
are used. Key records retain only transport arrows/J/K/L/Space/Escape, slash and
Shift+2 through Shift+8 for page-switch investigations; shortcut events record
key sequences but never entered text. Records include the current page and
keyboard layout to distinguish native shortcut collisions from focus changes.
The DLL remains loaded until exit.
An optional `-DPROBE_NAME=...` gives a revised passive probe a distinct DLL name;
this is not a way to replace the production helper in a running session.
