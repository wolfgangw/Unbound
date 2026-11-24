# Lower Color controls: remaining work

Supported controls, native expansion, active-mode sizing and validated Windows
startup restoration are documented in [AGENTS.md](../../AGENTS.md).

- Broaden macOS coverage across Color tool modes, clip/node changes and materials.
  General simultaneous updates, sizing, docking, layout recall and Color cold-start
  restoration are user-validated; see AGENTS.md.
- Broaden Windows coverage for Warper modes, clip/node changes and page/layout
  combinations. Larger modes must retain native minimums and allow later manual
  shrinking when the active mode permits it.
- Broaden main-window resize preservation checks to explicit recalls, manual
  floating resizes and other pages; basic Color startup, maximize/restore and page
  return are validated. Qualifier HSL compact sizing is user-validated; check larger
  modes as well.
- Validate 3D with stereoscopic material, including native controls and dragging.
- Check alternate native expansion/return routes and replacement-tool selection.
- Check macOS Scopes cold-start/layout restoration. Undocking, sizing, view-count
  controls, live updates, docking and page-return headers are user-validated.

Lower-region reclamation is deferred; retain native allocation. Broader native
replacement-tool selection validation remains open.

## Floating Color tools: viewer interaction routing

**Open, release-blocking:** clicking a floating Qualifier picker can leave viewer
strokes assigned to Magic Mask, Window or native-expanded Color Warper. With
Qualifier and Magic Mask both floating, either can retain the assignment despite
clicking the other. Qt focus and picker input arrive correctly; native tool context
is missing. Selecting the floating tool's native toolbar button, and consequently
replacing that region's docked tool with a floating placeholder, is now an accepted
limitation. The floating tools must remain usable and viewer input must reach the
tool the user chose.

### Control-by-control assessment

Windows native-UI checks distinguish **viewer gesture ownership** from controls
operated entirely inside a palette. A working slider, focus indicator or overlay
is not evidence that the next viewer gesture reaches that palette. The confirmed
failure affects more than Qualifier and Magic Mask; it is not a general failure
of floating grading controls or live image updates.

| Control | Assessment of this issue |
| --- | --- |
| Camera Raw | Material-limited: controls are disabled for the current non-RAW clip. Codec-specific controls remain unvalidated. |
| Color Match | Chart corner dragging works with the palette floating and Magic Mask docked, after explicitly selecting the native viewer **Color Chart** overlay, as documented. This does not establish automatic overlay restoration by floating-palette activation. Color matching itself requires chart footage and was not tested. |
| Color Wheels / Primaries | The floating picker needs the Color viewer's window active as its button release completes. With this handoff, white point changes Gain, black point changes Lift, and white balance changes Temperature and Tint in Color Bars while Magic Mask stays in the tray. Windows live trials cover all three with the viewer docked, white point with the viewer floating, white point in Color Wheels and white balance in Log Wheels. Check the installed build, Dual Screen and macOS. |
| HDR | Native HDR toolbar selection restores the zone overlay after Primaries takes the tray. Validate the floating activation path with Viewer Highlight enabled and the momentary wheel-range preview with Highlight off, including on macOS. |
| RGB Mixer | Local channel mixing; no analogous viewer picker or handle workflow identified. |
| Motion Effects | Local noise-reduction/motion settings; no analogous viewer gesture identified. |
| Curves | **Tested working:** floating Hue Vs Sat sampling creates curve points while Magic Mask remains docked. Native expansion is retained. Other curve modes were not exhaustively retested. |
| ColorSlice | Local vector grading and highlight preview; no viewer sampling gesture identified. Independent floating highlight-preview behavior remains unvalidated. |
| Color Warper | **Tested working:** floating Chroma Warp sampling creates its own stroke while Magic Mask remains docked. Native expansion is retained. Other Warper modes remain broader coverage work. |
| Qualifier | **Affected:** HSL/3D viewer sampling can reach the previously selected tool instead. Retained-container candidate does not resolve independent activation. |
| Window | **Affected:** clicking floating Window after selecting docked Magic Mask does not restore Window handles; the next viewer click creates a Magic Mask point. Docking restores the handles, and docked handle dragging changes Pan/Tilt correctly. |
| Tracker | **Affected in Window / Interactive Mode:** the floating control can generate visible tracking points, but clicking a point still creates a Magic Mask point. Stabilizer and FX tracking modes are not covered by this result. |
| Magic Mask | **Affected:** cannot reliably reclaim viewer gestures from floating Qualifier; both-floating competition is confirmed. |
| Blur | Local Blur/Sharpen/Mist controls; no analogous viewer gesture identified. |
| Key | Local matte/gain controls and an internal matte display; no analogous viewer picker identified. Sharing Qualifier's retained-container mechanism is not evidence of the same failure. |
| Sizing | Local transform controls. Native viewer overlay menu has no Sizing tool; the documented five sizing modes do not establish a palette-owned viewer picker. Reference-wipe interaction is separate and remains untested. |
| 3D | Material-limited: unavailable in the current ordinary 2D project. Stereoscopic/immersive interaction remains unvalidated. |
| Keyframes | Editing takes place in its own keyframe editor, not through tool-owned viewer strokes. Keyboard command routing is a separate mechanism. |
| Scopes | Signal display/settings, including viewer sampling readouts; not an owner of grading strokes. No evidence here of the tool-assignment failure. |
| Info | Read-only information; no viewer editing gesture. |

This is a complete catalogue assessment, **not** an all-modes regression pass.
Rows without an analogous gesture are classified from the inspected UI and manual;
they are not blanket functional passes. Resolve 21.1 manual references: Primaries
pickers pp. 3127–3128, Curves sampling p. 3192, ColorSlice pp. 3210–3212,
Color Match p. 3125, and Sizing pp. 3498–3500. Live sampling, mask and curve test
edits were undone; no layout snapshot was saved.

### Native-selection fix awaiting validation

On deliberate activation/input in an affected floating tool, select its catalogued
native toolbar button through `QAbstractButton::click()`. Keep the native tool
container in its original slot and transfer its layout contents into the host,
using the existing `retainedContainer` mechanism. Resolve then selects the tool
and handles viewer input itself. The corresponding tray region shows the floating
placeholder; another docked tool in that region is displaced. Other floating tools
remain open. No private handlers, focus IDs or forwarded viewer gestures are used.

Live Windows trials establish the following, beyond focus indicators alone:

| Tool | Required native container | Tested result |
| --- | --- | --- |
| Qualifier | `qualTabWidget` | HSL sampling changes its ranges after Magic Mask; floating Qualifier and retained floating Magic Mask alternate correctly in both directions. Native-toolbar 3D sampling also works. |
| Magic Mask | `personMaskTabWidget` | Viewer click creates its own point after floating Qualifier; Qualifier ranges stay unchanged. Toolbar selection without container preservation still misroutes the click. |
| Window | `windowWidget` | Returning from Qualifier restores the handles; dragging changes Window Pan/Tilt. Ordinary floating content loses the handles despite native toolbar selection. |
| Tracker | `trackerWidget` (already retained) | After docked Magic Mask, floating Window/Interactive mode displays tracking points and receives the viewer selection rectangle rather than creating a Magic Mask point. |
| Primaries | Unresolved | White-point picking still creates a Magic Mask point despite toolbar selection. Retaining `colorWheelsMainContainer` is insufficient; adding the existing command activation bridge to the ordinary host is also insufficient. |

The four secondary tools now use shared `ColorToolActivation` and catalogue-owned
container preservation; the mechanism is documented in AGENTS.md. The results above
come from live public-Qt trials. Shared fixtures pass offscreen and on the native
Windows backend, covering input ordering, ownership, automatic shows, guards and
reentrant docking. The integrated build still needs these live checks on Windows
and macOS:

1. Float Qualifier, select docked Magic Mask, then return to Qualifier and sample
   the viewer. Qualifier must change, without adding a Magic Mask point. Repeat in
   HSL and 3D modes, including add/subtract pickers.
2. Float Magic Mask too. Alternate between both tools several times, including
   header clicks and keyboard-only window return. Only the chosen tool receives
   the next viewer gesture; the tray placeholder changing is expected.
3. Float Window with an enabled shape. After choosing another tool, return and drag
   a viewer handle. Check handles return and Pan/Tilt/shape changes belong to Window.
4. Float Tracker in Window/Interactive Mode. After Magic Mask, return and select
   a region of cloud points in the viewer. Check for Tracker's dotted rectangle
   and that Magic Mask gains no point. Explicitly turn Interactive Mode off
   before testing another tool. In Windows Resolve, switching directly from
   docked Tracker to docked 3D Qualifier can leave the rectangle following
   the drag while Qualifier records its own sample. If the checkbox already
   appears clear, cycling it on and off with a Power Window enabled clears
   the rectangle. Floating Qualifier sampling after Edit → Color then works.
5. Alternate these tools with floating Curves and Warper. Sampling must return to
   the chosen tool, Curves/Warper must stay floating, and live updates must continue.
6. Check local editing, header/button dragging, tearoff, redocking and floating
   again. No lost input, unexpected activation/stacking or reset of local values.
7. Switch pages and recall layouts containing the floating tools, then restart
   Resolve. Check placement, preserved native tool choices, and sampling after
   explicitly choosing a floating tool. Automatic reveals must not choose a tool
   merely because it was shown.

Primaries white/black-point and white-balance picking are **not fixed** by this
build; local Wheels/Bars grading remains a separate working path.

Viewer OSC selection and local picker/submode changes are not substitutes for
native tool activation. Shared header controls do not imply native expansion
support; see the expansion boundary below.

The Resolve 21.1 manual documents Warper expansion (page 3223); no corresponding
Qualifier/Magic Mask expansion route was found in the manual or their native
options menus. Live public-Qt button inspection distinguishes visible native
Expand controls on Curves/Warper from Qualifier's explicitly hidden shared button.
The viewer's Qualifier overlay selects the sampling cursor (manual pages 3073–3074),
not a particular palette controller. Display Qualifier Focus belongs to Scopes
highlighting; it is not tool activation.

Native-expanded Warper, without an Unbound host, correctly alternates actual viewer
input with docked Qualifier: Warper's picker creates a pin; the docked Qualifier
picker updates HSL ranges. The coexistence behavior is native, but no equivalent
user-facing expansion route for Qualifier/Magic Mask has been established.

### Native expansion boundary

The inspected Windows session exposes three `UiPaletteSelectorWidget` wrappers:
Curves, Warper and Qualifier. Their Expand buttons emit
`WidgetDockStatusChanged(QWidget*,bool)`. With diagnostic listeners removed,
Curves and Warper each have one receiver; Qualifier has none, including when fully
docked. Calling the docked Qualifier's hidden button through `QAbstractButton::click()`
emits that signal but leaves it docked. The missing listener explains why exposing
the shared button does not enable native expansion. Unbound does not disconnect
this native signal.

Native Curves/Warper expansion moves the palette into one of two retained
`UiFloatingPaletteWidgetDialog` objects. Their inspected Qt metadata exposes
close/accept/reject handling, no generic tool-registration command, transferable
focus proxy or dynamic registration property. No equivalent user-facing expansion
action was found for the other affected tools. The existing native expansion route
therefore supplies no established portable fix for their viewer interaction.

Receiver counts use Qt's protected `QObject::receivers` accessor in a diagnostic
helper, without inspecting connection memory. These observations are not an API
contract or proof that no other native route exists. Do not forward another tool's
signal into Curves/Warper's registration or invoke private native handlers.

### Confirmed native notification path

Passive Windows tracing distinguishes three mechanisms:

- Docked tool input triggers `UiFocusedWidgetManager::FocusChanged(int,int)`.
  During that notification, `UiSecWidgetContainer` emits
  `CurrentSecWidgetMajorModeChanged(SmSecondaryCCMode)`. Qualifier, Window,
  Magic Mask and Warper have distinct observed focus identities. Do not hardcode
  their values or infer portable enum definitions from them.
- The major-mode notification precedes QWidget picker mouse-press delivery and
  Qt focus acquisition. No intervening WindowActivate is required. Floating
  Qualifier activation/picker input produces neither the manager notification
  nor the corresponding major-mode notification.
- Native-expanded Warper activation produces a separate floating focus identity
  and the major-mode notification. It can therefore take viewer interaction
  while another tool remains in the tray. Qualifier cannot reclaim it through
  ordinary floating focus. The original reported takeover without deliberate
  Warper interaction remains unexplained.

Qualifier HSL/3D changes emit `CurrentSecWidgetMinorModeChanged` through
`m_WidgetPanel::currentChanged`; clicking the already-selected mode does not emit
major-mode selection. Local picker signals likewise do not establish ownership.
No per-tool `Activated()` signal was observed in the docked input comparisons.

### Observation and next boundary

The native focus manager is not reachable through the inspected application/widget
child trees. A diagnostic listener can identify it from the container's active
QObject sender during the major-mode signal, then subscribe to its typed
`FocusChanged(int,int)` signal. This uses Qt metadata and its protected sender
accessor on the GUI thread, without private connection-memory traversal or Resolve
object offsets. The manager's inspected metadata exposes the signal, no callable
selection/registration slot. Keep this as diagnostic evidence, not a production
contract. Follow the live-probe restrictions in the viewer-probe README.

`SmSecondaryCCMode` has no registered Qt metatype in the tested session; observe
without consuming its argument. Do not guess its ABI or emit it as a command.
Neither application-only nor full-delivery FocusIn/WindowActivate probes on
Qualifier's retained wrappers/content produced the missing manager notification.
The inspected native dialogs/wrappers expose no dynamic focus-registration property
or focus proxy to transfer.

### Native region selection and window activation

The native three-region layout provides a useful baseline. Keyframes' **Split
Primary and Secondary Views** control exposes Primaries and Magic Mask together.
Clicking their pickers alternately routes actual viewer input correctly without
changing either region's selector or current stack page. Primaries changes Gain;
Magic Mask creates its own point. This establishes native coexistence across
regions, not independent selection of two tools belonging to the same region.

Retaining native ancestry, `qualTabWidget`, the complete `qualTab` root or its
exact original stack entry is insufficient. The command activation bridge can
restore a focus identity, red indicator and major-mode notification while actual
viewer input still edits Magic Mask. The failure also occurs with main's remembered
focus already on the viewer and no intervening major-mode notification. Complete
Primaries-root and primary-container trials similarly fail with ordinary floating
activation, even with Color Wheels still current in its native stack.

Public-Qt nonactivating-window trials distinguish two requirements:

| Retained native content, with main remaining active | Actual viewer result |
| --- | --- |
| Complete primary container; Color Wheels current, Magic Mask visible in the separate secondary region | White-point picker changes Gain correctly; native focus-manager notification returns. |
| Complete `qualTab`; original parent and stack entry retained, Magic Mask current in that same secondary region | Qualifier focus identity and major-mode notification return, but viewer input still creates a Magic Mask point. |
| Same floating `qualTab`, after explicitly selecting Qualifier in the native tray | Sampling updates HSL ranges correctly, but Magic Mask disappears from the tray. |

These diagnostic windows use `Qt::WindowDoesNotAcceptFocus`; Qualifier also uses
`WA_ShowWithoutActivating`. Verify actual `QApplication::activeWindow()` during
input: a nonactivating flag alone does not establish the test condition after an
activating show. No viewer gestures are forwarded and no private setters are used.
The trial is not a production focus policy: keyboard/text input, activation from
other floating windows, lifecycle behavior and macOS remain unvalidated.

The evidence separates native focus recognition from the region's selected tool.
Invoking the floating 3D Qualifier's own picker with the viewer window already
active still adds a Magic Mask point when Magic Mask is selected in that region.
Recovering the former does not independently select a different tool from the same
region. Native-expanded Curves/Warper retain an association that avoids this
limitation; its registration/selection mechanism remains unresolved. Neither focus
identity nor a major-mode signal proves the signal's argument or viewer recipient.
The accepted candidate above permits native tray switching and still requires
actual viewer edits as evidence. Do not invoke private Resolve setters, forward
gestures or hardcode focus IDs. Restore temporary ownership,
window flags and native layout changes before normal docking; undo test edits.

Viewer reclamation is tracked in [color-viewer.md](color-viewer.md). Fixtures cover
ownership, constraints and cancellation but do not replace native Resolve checks.
Public Qt size/restore diagnostics are documented in the
[viewer-probe README](../../tests/platforms/windows/viewer-probe/README.md).
