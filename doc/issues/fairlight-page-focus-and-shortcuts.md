# Fairlight page shortcut ambiguity

Status: unresolved native-shortcut comparison. Page-return focus restoration is
implemented and documented in AGENTS.md.

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
which can conceal activation-dependent behavior. Use physical user input with a passive
trace when verifying the failing sequence.
