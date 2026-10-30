# Dual Screen: remaining work

## Remaining compatibility validation

The shared logical-page association is implemented in `panepagecontext.h` and
documented in [AGENTS.md](../../AGENTS.md). Current validation there includes macOS
pane survival, subsequent docking, cold-start layout restoration and pinned ordering
against both native windows.

Remaining checks of implemented compatibility behavior:

- Include Edit Program/Source viewers in mode-exit and page-departure/return
  checks; their page association has a separate implementation path.

Fixtures cover page association for every catalogue entry, host visibility and
reversible docking on all supported pages, auxiliary startup discovery and Edit
viewers initialized outside main's tree. These do not replace native validation.

The release scope is safe coexistence and surviving exit to Single Screen.
Full Dual Screen layout integration, separate mode-specific layouts, native
allocation redesign and a broader floating-window workflow are deferred.

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
