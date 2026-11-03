# macOS keyboard-routing validation handoff

Read `AGENTS.md` and [Floating-pane keyboard routing](floating-pane-keyboard-routing.md).
Build and validate the shared activation bridge on macOS in a fresh Resolve
process. Replacing a loaded helper does not update the running session.

Use the Windows coverage as the regression checklist, establish native docked
baselines where needed, and record macOS results separately in the routing document.
Include pane command routing, switching subcontrols within an active floating pane,
page/layout return, docking/refloating, tearoff, dialog dismissal, application
switching, minimize/restore, keyboard-only panel selection, viewer routing,
Single/Dual Viewer mode changes, Cinema return and Dual Screen Mode.

Use disposable test content and clean up afterward. Verify that commands affect
the intended recipient and leave unrelated selections intact. Preserve user
preferences, keyboard mappings and saved layouts. Ask the user for checks that
automation cannot reliably prove, especially keyboard-only return when automated
key delivery would activate the target window and mask a focus failure.

## Cinema Viewer caveat

The Cinema Viewer return fix is Windows-specific. Its adapter preserves native
return focus across restoration of the main window, preventing floating Program
transport from falling back to docked Source. Verify macOS behavior independently;
do not copy the Windows workaround merely because it was needed there.

With Program floating and Source docked, check both sequences without another
click after returning:

- Click Program, enter Cinema Viewer, exit it, then use J/K/L.
- Click Program, switch to another application, return directly to Program, then
  use J/K/L.

Both should control Program/timeline transport. Repeat in Dual Screen Mode where
available. Use the native macOS shortcuts/current mappings rather than assuming
Windows key combinations. Windows user validation passes both sequences in
Single Screen and Dual Screen configurations; that is not proof of macOS behavior.
