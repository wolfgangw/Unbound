# Dual Screen: remaining work

## Floating-panel disappearance

Native Windows reproduction confirms that Edit Media Pool's dock slot moves
between the main page and `frameEditAuxWidgetsContainer` under `UiEditPanelAux`.
Floating from the auxiliary container bound visibility to that container; disabling
Dual Screen hid the host while preserving its content and floating state.

The shared logical-page association is implemented in `panepagecontext.h` and
documented in [AGENTS.md](../../AGENTS.md). Edit Media Pool surviving Dual Screen
exit is user-validated on Windows. Remaining native validation:

- Start directly in Dual Screen; float panes on Edit, Color and Fairlight, then
  disable it. Include Media Pool/Index, Color Gallery/LUTs and Fairlight Mixer.
- Repeat both directions, including page departure/return, docking and Dock all.
- Preserve geometry, focus, native content and saved layouts across these moves.
- Check Full Screen Timeline, actual multiple monitors and macOS.
- Exercise both Edit viewers, native Color palettes/Scopes and Clips/Lightbox;
  native mode-specific initialization/allocation is separate from page association.
- Check Fairlight Media Pool's audition ownership and keyboard routing.
- Validate Always on top against activation of both native screens, menu use,
  application switching and minimization. Shared group membership includes the
  discovered auxiliary wrapper; native cross-platform validation remains pending.

Read-only native snapshots identify all three supported auxiliary windows.
Fixtures cover page association for every catalogue entry, host visibility and
reversible docking on all supported pages, auxiliary startup discovery and Edit
viewers initialized outside main's tree. These do not replace native validation.

Full Dual Screen layout integration and separate mode-specific layouts remain
outside the scope of this compatibility fix.

## Native Color Clips / Lightbox constraints

Windows testing with both Resolve windows on one physical display establishes:

- Turning primary Clips off exposes secondary Lightbox; Gallery and Keyframes
  can coexist with it.
- Turning Clips on removes Lightbox. If it was the sole secondary content,
  Resolve shows Gallery, Keyframes and Scopes instead.
- Hiding sole Gallery or Keyframes with Clips on reveals the other. The secondary
  window therefore retains content even when the user disables its last panel.
- The same `thumbnailView` (`UiVSRWidgetContainerImp`) and `splitterThumbnail`
  move between the primary strip and secondary Lightbox. Visibility toggles cannot
  display that widget in both places simultaneously.

Independent Clips/Lightbox duplication is deferred: no supported independent view
and data interface is established. Selected clip and playhead survived tested
toggles. Actual two-monitor placement and other platforms remain unvalidated.
