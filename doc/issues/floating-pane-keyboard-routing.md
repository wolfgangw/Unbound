# Floating-pane keyboard routing

## Problem

A floating pane can have Qt focus and a visible selection while Resolve retains
another command context. Delete may do nothing or act on unrelated timeline/tool
content. Confirmed cases include Nodes, Media Pool bins, keyframe editors, Gallery
and Color Timeline markers. Working text entry or Alt+S does not prove correct
Delete routing.

Sound Library has a separate audio-selection issue: after returning from Program
through its header or Alt+Tab, audition transport can move silently because the
preview never becomes the active audio player. This is not established as a regression
from the broader command bridge.

## Implemented correction

- `PaneActivationBridge` restores the logical page's command context once on genuine
  floating-host activation; native focus selects the command recipient. No key
  interception/forwarding or per-click notifications.
- Catalogue coverage includes Media Pool on all supported pages, Edit Keyframe Editor,
  Color Nodes/Gallery/Timeline/Keyframes, Sound Library on Edit/Fairlight and both Edit
  viewers. Text-only controls, native palettes and Scopes do not receive a blanket bridge.
- `AuditionPlayerActivation` briefly visits Sound Library preview focus after activation,
  then restores the selected control. This matches native docked audio activation while
  preserving result-list/search focus. Native header focus need not imply list focus.
- Windows Cinema return preserves the viewer selected by Resolve across main-window
  restoration. macOS passes the tested return paths without this platform workaround.

Ownership, lifecycle guards and implementation details live in
[AGENTS.md](../../AGENTS.md#pane-command-activation-and-audition-players).

## Validation state

| Area | Windows | macOS |
| --- | --- | --- |
| Contextual pane commands | Nodes, all Media Pools, Edit/Color keyframes, Gallery and Timeline marker deletion pass; unrelated selections retained in tested cases | Nodes, Edit/Fairlight bin deletion, Edit rename and Edit keyframe wrong-recipient protection pass |
| Lifecycle | Startup, page/layout return, dock/refloat, tearoff, popup/dialog return, app switching, keyboard-only panel selection, minimize/restore pass on representative panes | Nodes startup, dock/refloat and page return pass; tearoff deletion without another click pending |
| Edit viewers | Source/Program selection, floating combinations, Single/Dual Viewer changes, Cinema and Alt+Tab return pass; tested Dual Screen paths pass | Program Cinema/application return with Source docked pass; broader mode and Dual Screen coverage pending |
| Sound Library audio | Installed fix user-confirmed on Edit and Fairlight; Edit trace confirms preview activation, retained focus and list navigation after header/Alt+Tab return | Installed shared audio correction user-confirmed on Edit and Fairlight after header/application return |

These are representative checks, not every command on every pane. Local editing
in tested Metadata/Color fields, Clips navigation and Color viewer transport work
without an added bridge.

## Remaining checks

macOS sign-off: Color Nodes Delete immediately after tearoff without another click. Follow the [macOS handoff](floating-pane-keyboard-routing-macos-handoff.md).

Broader coverage: Sound Library remembered list/search focus on macOS, keyframe modes/multiple selection, Gallery albums/memories, Clips
grade clipboard targets, viewer tools, custom mappings and remaining Dual Screen
combinations. Establish a docked baseline, use disposable content, and keep an
unrelated selection to detect wrong-recipient actions. Verify audible output separately
from transport motion. Automated input that activates its target cannot prove
unassisted return focus.

## Reference

Resolve 21.1 manual: focus pp. 55/110; Application versus Panels shortcuts pp. 123–124;
Edit panel selection p. 795; Color focus p. 3047; Sound Library transport p. 3785.
The local PDF/text extraction are under `doc/resolve/`. Keyboard Customization exposes
panel contexts but not every native field/player action; a category or key label is
not proof of the final recipient. Preserve user mappings/preferences.

Passive audio tracing: [diagnostic README](../../tests/platforms/macos/audition-audio/README.md).
