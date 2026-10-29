# Dual Screen: remaining work

## Floating-panel disappearance

An unresolved Windows report describes Media Pool and Index becoming inaccessible
after undocking in Dual Screen and returning to Single Screen. Returning to Dual
Screen reveals them; docking and undocking in Single Screen clears the condition.
The report concerns Unbound 0.2.0 and Resolve 20.3.1. The page, Full Screen Timeline
state and display arrangement are unknown; native reproduction is still needed.

`SidebarFloatSession` retains its original page container for Hide/Show tracking.
It does not rebind when the native slot changes containers. Fixtures confirm that
moving an Edit Media Pool/Index slot into another visible container and hiding the
old container leaves its active host hidden. Moving the original container intact
between top-level windows does not reproduce this failure. This establishes a
lifecycle weakness, not the cause of the native report.

Capture slot/container ancestry across a failing native switch. Distinguish hidden
hosts from off-screen placement, hidden content and slot destruction/replacement.
If container replacement is confirmed, rebind visibility tracking while preserving
host, content, geometry and settings. Actual page departure must still hide hosts;
do not add polling or unconditional reveals. Fairlight Media Pool's audition
ownership requires separate validation.

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
