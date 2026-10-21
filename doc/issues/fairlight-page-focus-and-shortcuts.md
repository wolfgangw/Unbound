# Fairlight transport focus and page shortcuts

Status: focus correction implemented with native Windows regression coverage;
Fairlight timeline/Media Pool transport focus confirmed live on Windows.
macOS validation pending. Native shortcut collision is unchanged.

## Report

After returning to Fairlight, Space does not start timeline playback until the
timeline is clicked, even when the timeline had focus before leaving the page.
Separately, Shift+7 fails to enter Fairlight from Media, Cut and Edit, but succeeds
from Fusion, Color and Deliver.

## Findings

These are two distinct mechanisms.

### Returning floating windows take focus

A passive Qt/Win32 trace shows Resolve initially restoring Fairlight timeline
focus. As Unbound reveals the retained floating Mixer, Media Pool and Effects
windows, native activation moves through those hosts. Focus ends on
`btnEffectToggleFolderView` in
`unboundFloating_frameAudioPanelEffectsLibraryContainer`.

Space still activates Resolve's native, non-ambiguous Space shortcut, but with
the floating Effects window active. Clicking the timeline restores
`UiMainWindow` activation and timeline focus; Space then controls playback.
This is a window-activation problem, not a missing Space event.

Previously, `SidebarFloatSession` set `Qt::WA_ShowWithoutActivating` around page-return
`show()` calls. In Qt 5.15.2, `QWidgetPrivate::create()` copies that attribute to
the backing QWindow's `_q_showWithoutActivating` property. The Windows backend
reads that property when showing a window. Setting the QWidget attribute after
native creation does not update that property in this Qt version. Unbound
created native handles early for taskbar identity, before setting the attribute.
Edit viewer hosts also used ordinary show/setVisible calls on page return.

Both host backends now set the attribute in their constructors, before native
creation, and retain it. Explicit manual undocking activates the new host;
automatic restoration and subsequent reveals do not.

Shared `pagefocus.h` remembers the user's last window/control per page within
the session. It records stable focus changes, ignoring hide/show churn and the
main-window activation caused by clicking the page bar. After queued page reveals,
it recalls an available target or falls back to Resolve's native main-window
focus. New input, app deactivation and superseding switches cancel pending recall;
dialogs and other application windows retain focus. No settings fields or
transport shortcut handlers were added.

The shared layout fixture exercises main, panel and viewer focus, passive native
show, navigation activation, unavailable targets, destroyed controls, rapid page
switches, newer clicks, application deactivation and modal dialogs. It runs with
the native backend; the offscreen backend cannot model window activation.
Live checks should cover timeline transport, Media Pool audition, explicit clicks,
dragging, taskbar activation and manual undocking on both platforms.

The native fixture must activate main before hiding the outgoing active host,
matching the native focus handoff. Hiding an unowned active fixture first can
activate another application: a captured ApplicationDeactivate then correctly
cancels recall, producing a misleading viewer-restoration failure. The fixture
retains separate assertions that genuine deactivation cancels pending recall.
Synthetic layout clicks also require confirmed native and Qt activation before
delivery, rather than assuming a fixed delay completed the foreground handoff.
The single observed popup failure was not reproduced under tracing; its precise
cause remains unconfirmed. Activation preconditions now fail explicitly, and
independent gesture fixtures run in separate processes to avoid teardown overlap.

Qt source references:

- [QWidget native creation, Qt 5.15.2](https://github.com/qt/qtbase/blob/v5.15.2/src/widgets/kernel/qwidget.cpp)
- [Windows show handling, Qt 5.15.2](https://github.com/qt/qtbase/blob/v5.15.2/src/plugins/platforms/windows/qwindowswindow.cpp)

### Shift+7 collides with slash

The actual user-input trace identifies the active keyboard layout as German
(`0407`). Physical Shift+7 reaches Resolve as expected. On Media, Cut and Edit,
Qt delivers an ambiguous `/` shortcut event to Resolve's `UiMainMenuBar`; on
Color and Deliver it delivers a non-ambiguous `Shift+7` event and changes page.
The trace reproduces the failure on Media and Cut with the native main window
active, not an Unbound host.

Resolve's menu contains both the Shift+7 page shortcut and the `/` playback
shortcut. Playback > Play Around/To labels `/` as Play Around Current Selection.
Their page-dependent availability explains the reported pattern. Unbound does
not register either shortcut. A fresh session without Unbound was not used for
this comparison, so do not describe it as an unloaded baseline test.

Remapping one of these native shortcuts is the appropriate next comparison;
Unbound should not override Resolve's global keyboard handling.

## Inspection

The bounded probe in `tests/platforms/windows/cinema-focus/` also records page,
keyboard layout, Space, slash and shifted page-number keys. It never consumes
events or changes focus. Capture shortcut and transport sequences separately:
many page switches can exhaust its event limit.

Computer-use key delivery activates its target window before sending input,
which can conceal this focus defect. Use physical user input with a passive
trace when verifying the failing sequence.
