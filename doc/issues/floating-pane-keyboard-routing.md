# Floating-pane keyboard routing

## Scope and evidence

Audit source: [DaVinci Resolve 21.1 Reference Manual](../resolve/DaVinci%20Resolve.pdf)
and its [complete text extraction](../resolve/DaVinci%20Resolve.txt).
References below are manual page numbers, also the one-based PDF page numbers in
this copy. The extraction preserves all 4,351 page boundaries; screenshot text,
diagrams and multicolumn tables may require consulting the PDF.

The manual establishes expected behavior, not which Unbound panes are broken.
Review covered general focus/customization guidance and the relevant Media Pool,
metadata, Edit, Color and Fairlight chapters, with full-text searches for keyboard
commands, focus, selection, navigation, deletion and text entry. It is not a complete
inventory of commands available in the running version's Keyboard Customization UI.

Current Windows evidence:

- Floating Color Nodes receives Qt focus and Delete key events, but Resolve invokes
  a tracking-related action instead of deleting the selected node. Docking restores
  the expected behavior. A temporary use of the existing activation bridge restores
  node deletion without forwarding keys or moving focus.
- Floating Edit Media Pool fails to delete a selected disposable bin with Backspace;
  docked operation works. The same bridge restores bin deletion and the native
  selected-clip deletion dialog. Inline rename Backspace still edits text. A selected
  timeline clip remains intact when deleting the bin with the bridge.
- Floating Edit Keyframe Editor: Delete on a selected temporary opacity keyframe
  deletes the selected timeline clip instead. Docked Delete removes only the
  keyframe. A scoped temporary activation bridge restores keyframe-only deletion
  while the timeline clip stays selected and intact. The unintended timeline
  deletion was immediately undone and the restored clip verified.
- Color Media Pool: Backspace removes an empty test bin when docked but leaves it
  when floating. A scoped activation bridge restores deletion. Keep bin selection
  separate from inline rename: clicking an already-selected label can enter rename,
  where Backspace correctly edits text instead. The temporary bin was removed.
- Within one continuous floating Color Media Pool activation, the bridge preserves
  native subcontrol routing: bin-name Backspace edits text, Escape cancels rename,
  browser Right changes clip selection, tree Backspace deletes the selected empty
  bin, then browser Delete removes the selected clip. Clip removal was undone and
  all eight original clips verified restored. A passive host/focus trace confirms
  no intervening host reactivation across that sequence. No per-click or child-focus
  bridge notification was needed. The temporary bridge was removed and the pane's
  original docked/hidden state restored; other pane types remain separate coverage.
- Fairlight Media Pool: Backspace removes an empty test bin both docked and floating
  with the existing production audition bridge; no additional bridge was installed.
  The temporary bins were removed. This covers bin-tree commands, not all preview
  transport or browser operations.
- Floating Edit Metadata: Description typing, Backspace, Select All and Delete
  work without a bridge. Selecting and deleting a temporary keyword token also
  works. Temporary text and the keyword were removed. This does not validate
  custom Metadata commands or the Fairlight instance.
- Color Keyframes: a selected static keyframe deletes when docked but remains when
  floating without a bridge (continued presence also checked by the user). The
  scoped bridge restores deletion. The temporary keyframe was removed and the pane
  returned to its original docked state.
- Color Timeline: Delete removes a selected temporary marker when docked, but the
  marker remains when floating. The scoped bridge restores marker deletion. The
  temporary marker was removed and the pane returned to its original floating state.
- Color Gallery: Delete removes a selected temporary still when docked but leaves
  it when floating. The scoped activation bridge restores deletion. Both temporary
  stills were removed; the original three stills remain. Album/memory operations
  and grade application are not covered by this deletion check.
- Color Clips/Lightbox: Down advances the selected clip in both docked Lightbox
  and the floating grid without an added bridge. This does not cover grade
  clipboard destinations, multi-selection or destructive commands.
- Color Viewer: Right advances one frame both docked and floating; floating L/K
  starts and stops playback without an added bridge. On-image tool commands remain
  untested. The viewer was returned to its original docked state.
- Floating Color Wheels: the Primaries Temperature editor accepts text entry,
  Backspace and Select All without an added bridge. Its original 0.0 value was
  restored. This covers local numeric editing, not page-level grading commands.
- These observations do not establish failure on every page or pane. macOS coverage
  remains open. Edit keyframe testing covers a single opacity keyframe in the
  Keyframe view; Curves mode and multiple-keyframe operations remain untested.
- Existing production activation handling covers Edit Source and Edit/Fairlight
  audition players. Its mechanism is documented in AGENTS.md under Audition players.
  Shared integration now includes the confirmed command-sensitive panes through
  catalogue policy and the normal floating-session lifecycle. Fresh-process Windows
  lifecycle validation is partially complete below; macOS coverage is listed separately.

## Integrated lifecycle coverage

Fresh-process Windows testing uses the installed shared integration, without
temporary activation bridges or diagnostic hooks.

| Scenario | Verified command and result |
| --- | --- |
| Cold-start floating restoration | Color Nodes: Delete removes a temporary serial node, retaining the original node. |
| Page departure and return | Color Nodes: the same command works after an Edit/Color round trip. |
| Dock and recreate host | Color Nodes: docked and newly floating deletion both work. Edit Media Pool: closing the host docks it; refloating and Backspace removes only a disposable empty bin. |
| Application departure and return | Color Nodes: deletion works after switching through File Explorer and returning directly to the floating host. This is not a keyboard-only Alt+Tab test. |
| Keyboard-only Alt+Tab return | User verified Color Nodes: add a temporary serial node, Alt+Tab to another application, Alt+Tab directly back to Nodes, then Delete without clicking. Only the temporary node disappears. |
| Keyboard-only panel selection | User verified Edit: from timeline focus, Ctrl+1 then Down moves selection in the floating Media Pool bin tree; Ctrl+2 then Right moves clip selection, without clicking Media Pool. |
| Layout recall | Color All Docked / All Out Snapped: deletion works after floating hosts are recreated. |
| Native popup dismissal | Color Nodes: deletion works after opening its context menu and dismissing it with Escape. |
| Main minimize and restore | Color Nodes: deletion works after restoring main and activating the floating host. |
| Wrong-recipient protection | Edit Media Pool: temporary-bin Backspace leaves the separately selected timeline clip and all eight original browser clips intact, including after host recreation. |
| Tearoff release | User verified Color Nodes without a subsequent canvas click: a temporary node added while docked deletes after tearoff; Alt+S/Delete immediately after tearoff also works. |
| Native dialog return | User verified floating Color Nodes: open Keyboard Customization, close without changes, then Alt+S/Delete adds and removes the temporary node without clicking Nodes again. |
| Color Keyframes integration | Floating Keyframes: Delete removes a selected temporary static keyframe; the track is empty again after docking. |
| Edit Keyframe Editor integration | Floating opacity-keyframe Delete works both after initial undocking and an Edit/Color/Edit round trip. The selected timeline clip remains intact; temporary keyframes are removed. |
| Edit audition transport | Floating Sound Library L/K starts/stops audition playback while the timeline remains stationary. Clicking the timeline then L/K operates timeline playback; audition remains at its stopped position. |
| Color Gallery and Timeline integration | Floating Gallery Delete removes a temporary still, retaining all three original stills. The retained floating Timeline deletes a selected temporary marker after page return. |
| Fairlight audition transport | Floating Sound Library L/K starts/stops audition playback; clicking the timeline then L/K controls timeline playback while audition remains stopped. Floating Media Pool preview responds to J/L and K while the timeline remains stationary. |
| Edit Source transport | In native Single Viewer mode, floating Source responds to J/L and K. Clicking the timeline returns transport to the timeline; Source retains its stopped position. Source is redocked afterward. |
| Edit Dual Viewer transport | Floating Source with docked Program, and both viewers floating: selecting Source plays Source without moving the timeline; selecting Program plays the timeline. Source playback also works after an Edit/Color/Edit round trip with both floating. |
| Floating Program with docked Source | With both viewers using the bridge, clicking Program makes J/L/K control timeline playback while Source stays unchanged. Clicking docked Source returns playback to Source without moving the timeline. Program routing also works after an Edit/Color/Edit round trip. |
| Program Cinema Viewer | Entry hides the normal windows; Escape restores them. Transport works in Cinema. User validation confirms floating Program → Ctrl+F → Escape → J/K/L controls Program without another click, with Source docked. The Windows Cinema adapter preserves native return focus across main restoration; fixture checks also pass. |
| Single/Dual mode changes while floating | Dual to Single with Program floating and Source docked retains Program L/K transport. With both floating, Single hides Source and returning to Dual restores it; selecting Source then J moves only Source, and selecting Program then L/K moves/stops only the timeline. Original Dual mode with Source docked is restored. |

Temporary nodes, bins, keyframes, stills and markers have been removed. These checks cover representative
lifecycle paths, not every command in every pane. The tested Windows lifecycle
paths pass, including keyboard-only Program Cinema return, successful manual
tearoff and ordinary dialog-return activation. Both Edit viewers
now bind the shared activation bridge. Fresh-process Windows validation confirms
the correction for floating Program incorrectly controlling docked Source despite
Program's red focus indicator. Single/Dual mode changes pass for the combinations
above; other viewer lifecycle combinations remain separate coverage.
Dual Screen checks pass on Windows for Edit Media Pool detached from the auxiliary
window: temporary-bin Backspace works initially, after an Edit/Color/Edit round trip,
and after docking/refloating. Color floating Nodes also passes Alt+S/Delete.
With Program floating and Source docked, Program J/K controls timeline playback
without moving Source; selecting Source then L moves Source without moving the
timeline. Turning Dual Screen off and back on retains both floating hosts and
Program frame-step routing. Temporary bins/nodes are removed; original media remains.
User validation also confirms keyboard-only return in Dual Screen: floating Program
to Cinema (Ctrl+F), Escape, then J/K/L controls Program/timeline without another
click; Alt+Tab away and directly back to floating Program likewise retains
Program/timeline transport without another click.
Fairlight and other Dual Screen configurations remain separate coverage.
macOS remains separate coverage.

### Open Sound Library audio activation issue

The user reproduces silent audition after returning from docked Program to floating
Edit Sound Library through its header or Alt+Tab on both Windows and macOS.
Transport can advance while Program remains Resolve's active audio player.
Windows passive signal tracing confirms that waveform focus deactivates Program
and activates the preview; restoring container/results-list focus can omit that
step. Native docked header return briefly focuses the preview before settling on
the container. A first floating return may work when Qt still remembers preview
focus, so repeated returns with different remembered subcontrols are required.

Earlier transport checks do not establish audible return through every activation
path. A container-only Qt focus proxy is insufficient; it does not cover remembered
results-list focus. A temporary preview-focus visit followed by restoration of the
selected control is user-validated for consistent audio on Edit header/Alt+Tab
return. Shared integration is now implemented for Edit and Fairlight Sound Library.
Fresh-process Windows Edit validation confirms audible header and Alt+Tab return
by user listening. Passive tracing independently confirms Program deactivation,
preview activation and restoration of container/results-list focus on both paths,
including repeated header return after result selection. L/K controls audition
without moving the timeline; Up after Alt+Tab with retained list focus selects the
previous result. Windows Fairlight Sound Library is also user-confirmed working.
macOS validation remains pending. The native docked comparison confirms
Up/Down after a header click navigates the timeline, while clicking the result list
then Down selects the next result. Do not change this native distinction. Validate audible
output separately from transport movement and activation signals, and retain
search editing, list navigation, selection and playback position.
The expanded bridge has not been established as the cause;
Sound Library already used this activation mechanism before the expansion.

### macOS validation

Fresh-process testing of the installed shared integration confirms floating Color
Nodes Backspace deletes a selected temporary serial node while retaining the
original node in a restored floating host, after docking/refloating, and after an
Edit/Fairlight/Color round trip.

Floating Edit and Fairlight Media Pool: Workspace > Active Panel Selection >
Media Folders reaches the bin tree, arrow keys change its selected bin, and
Backspace removes a selected disposable empty bin while retaining the original
three bins. Edit inline bin rename also accepts text and Backspace without
deleting the bin. These checks do not establish protection of a separately selected
timeline clip, browser clip deletion, audition transport or continuous activation
across subcontrol changes.

Temporary nodes and bins were removed. These checks used the separate validation project
and the existing DaVinci Resolve keyboard preset, without mapping changes.

User validation confirms both Edit Program return paths with Program floating and
Source docked: entering/exiting Cinema Viewer, and switching to another application
and returning directly to Program. Without another click, J/K/L controls
Program/timeline transport and leaves Source unchanged. macOS therefore passes
these checks without the Windows-specific Cinema return workaround. Dual Screen
repetition remains pending.

User validation confirms Edit Keyframe Editor wrong-recipient protection: with a
timeline clip selected, Delete removes a selected opacity keyframe in the floating
editor while the timeline clip remains intact.

Edit Sound Library user validation confirms J/K/L targets the focused floating
audition player, docked Program viewer or docked Source viewer. Returning directly
to the floating preview waveform after either viewer retains audible audition
without clip reselection; a passive trace confirms focus returns to the preview
player. Returning via the floating header after either viewer reproducibly leaves
transport silent; the trace shows focus on the Sound Library container rather
than its preview player, with the bridge notification delivered in both paths.
Clip reselection restores sound. The exact docked header-click comparison retains
audible playback. Preserving native header press/release is insufficient to restore
audible return. Native signal observation shows Program remains the active player
during a silent floating-header return; clicking the waveform deactivates Program
and activates the audition player, restoring sound without clip reselection. No
mute-change signal was observed in that capture. In the working docked header
return, native focus briefly visits the preview, Program emits inactive and the
preview emits active, then focus settles on the Sound Library container. The
silent floating return omits that preview activation. The shared correction
described above still needs fresh-process macOS validation, including header and
application return with result-list or search-field focus retained. Do not
generalize waveform-return success to header activation.

The passive diagnostic and its reproduction procedure are preserved in
`tests/platforms/macos/audition-audio/README.md`.
Rebuild/reinstall from source in a fresh Resolve process before validating the
production correction. Raw traces remain local under
`build/macos/audition-audio/`; they are not shipped or committed.

The remaining macOS sign-off checks are resolving the Edit audition audio issue,
Sound Library audition versus timeline transport on Fairlight, and Color Nodes
deletion immediately after button tearoff without another canvas click. Other combinations remain additional
coverage rather than prerequisites for this scoped integration sign-off.

Other panes and lifecycle checks remain pending. Computer Use window targeting is
currently unreliable; ordinary input preparation must not be counted as proof of
unassisted application, dialog or Cinema return focus.

## Resolve's documented routing model

- Clicking within a panel gives it focus and directs shared shortcuts there
  (pp. 55, 344, 796, 1288, 3047). Selection alone does not establish command routing.
- Keyboard Customization distinguishes Application and Panels commands. Panel-local
  actions take priority when the panel is focused (pp. 123–124). Media Pool, Edit
  Timeline, Metadata Editor and Sound Library are explicit examples. Custom mappings
  mean default-key tests alone cannot establish compatibility.
- Workspace > Active Panel Selection and shortcuts can move focus without a click.
  Edit lists Media Folders, Media Clips, Source Viewer, Timeline, Timeline Viewer,
  Effects, Edit Index and Inspector; Q switches viewers (p. 795).
- Focus indicators expose the intended active panel (pp. 55, 110), but are not proof
  that every native command is routed correctly after reparenting.
- Undo has different scopes: Media/Edit/Fairlight share a stack; Fusion and Color
  have per-clip stacks (p. 69). Undo must be checked against the intended operation.
- Examples below use Windows Ctrl/Alt for the manual's Command/Option (p. 68).
  Preserve distinctions between Backspace, Delete and the manual's Mac Forward-Delete;
  establish actual keys from the current mapping rather than treating them as aliases.

## Supported-pane audit

“Command” means documented keyboard behavior affecting that content; it does not
necessarily mean a panel-local shortcut. “Fields” means local text/numeric input.
“Unspecified” means no dedicated panel shortcut was established by this review,
not that the pane cannot receive shortcuts. All remain in regression coverage.

### Edit and shared panes

| Pane / pages | Documented behavior and source | Validation target |
| --- | --- | --- |
| Media Pool — Edit, Color, Fairlight | Delete/Backspace removes selected media (p. 372) and bins (p. 377); arrows select, Return opens Source (pp. 867, 896); marker-list arrows (p. 1024); rename/search fields | Test bin tree, clip browser, search and rename separately on every page. Fairlight adds its own preview player (pp. 3774–3775). Bin failure confirmed on Edit and Color; Fairlight passes with its existing bridge. |
| Sound Library — Edit, Fairlight | Explicit panel mapping category (p. 123); Space and JKL audition playback (p. 3785); search text (pp. 3783–3784) | Search versus results versus player; returning to timeline must restore timeline transport. Existing bridge needs regression coverage. |
| Source and Program viewers — Edit | Active-panel shortcuts/Q (p. 795); Space, JKL, frame/clip navigation, marking and timecode (pp. 870–879); multicam commands (pp. 1043–1044) | Source versus timeline playback, both floating, native single/dual viewer modes, editable timecode and on-image tools. |
| Keyframe Editor — Edit | Cut/copy/paste and Delete selected keyframes, assignable nudge commands (p. 1258); curve-point deletion (p. 1262) | Both keyframe and curve modes; selected timeline clips must not be deleted instead. |
| Metadata — Edit, Fairlight | Explicit panel mapping category (p. 123); keyword arrows/Return (p. 408), Delete selected keyword (p. 409) | Empty/selected states, text editing, keyword selection and custom panel mappings. |
| Inspector — Edit, Fairlight | Edit active-panel target (p. 795); numeric typing/Return (p. 1153); Fairlight parameter entry and bracket-key keyframe navigation (pp. 3921–3922) | Text/numeric fields versus parameter controls; changes must affect the intended clip/track, including effect tabs. |
| Edit Index — Edit | Active-panel target (p. 795); search (p. 1030) | Search and row selection. Do not infer Delete removes a row/clip merely because it is selected; establish docked behavior first. |
| Effects — Edit, Fairlight, Color | Edit active-panel target (p. 795); Effects Search shortcut (p. 1159); Color effect keyframe navigation (p. 3493) | Browser/search versus effect parameter editor. Opening Effects Search is not proof of browser-local routing. |
| Mixer — Edit, Fairlight | Fairlight track selected in Mixer can be Smart Zoom target (p. 3754); channel controls (pp. 3767–3770); assignable Solo Safe command (p. 3965) | Channel selection, editable values/names, intended track for commands, native plugin dialogs. Do not presume every mixer action has a local key binding. |
| Video Scopes — Edit; Scopes — Color | Workspace Video Scopes toggle Ctrl+Shift+W (p. 3093) | Opening/closing is an application action. No scope-local destructive shortcut established. Check focus return, transport and settings fields for regressions. |

### Color-specific panes

| Pane | Documented behavior and source | Validation target |
| --- | --- | --- |
| Nodes | H toggles pan (p. 3388); node navigation (p. 3391), enable/reset (pp. 3392–3393), add/delete (p. 3395), link deletion/extraction (p. 3396), copy/paste (p. 3397) | Nodes, links, graph background and label editor separately. Alt+S working does not prove Delete has the right command scope. |
| Clips / Lightbox | Clip navigation (pp. 3106–3107), sort command (p. 3112); grade copy/paste versus node paste explicitly depends on clicking Nodes (p. 3362) | Selected clips, multi-selection, grade clipboard destination, clip navigation and Lightbox mode. |
| Timeline | In/Out and clip navigation (p. 3107); Delete selected Mini-Timeline marker (p. 3114) | Marker versus clip selection, transport and timecode. Include this destructive target in wrong-recipient tests. |
| Viewer | Zoom keys (p. 3076), arrows/Space/JKL (p. 3077); Window points Backspace/Escape (p. 3275) | Playback, timecode and active on-image tool are distinct contexts. |
| Gallery | Previous/next still and wipe commands (p. 3336); album rename (p. 3341); grade memories (pp. 3347–3348, 3364–3365) | Still/album selection, rename text and grade destination. Application-accessible still commands do not prove Gallery-local focus. |
| LUTs | LUT browsing/selection and application (chapter 150, pp. 3476–3485) | Browser/search, selection and native application commands. No dedicated browser-local key command established. |
| Color Wheels / Primaries | Numeric fields support arrows and clipboard (p. 3145); Printer Light Hotkeys (p. 3158) | Field editing versus page-level printer-light mode; preserve native hotkey enable state. |
| ColorSlice | Numeric fields support arrows and clipboard (p. 3212) | Local field editing must not trigger clip navigation or grade clipboard commands. |
| Window | Backspace deletes selected curve control points; Escape deselects (p. 3275) | These are Viewer interactions associated with the palette; test viewer/palette focus transitions. |
| Tracker | Track forward/reverse/stop keys (pp. 3308–3309); interactive point deletion controls (pp. 3314–3315) | Tracking command context and Viewer points. The manual describes a Delete control but does not establish its keyboard binding; do not infer that binding from its label. |
| Keyframes | Keyboard Start Dynamic/Add Static scope (p. 3455); Alt+] deletes at playhead (p. 3463); node-based keyframe copy/paste (p. 3464) | Track/mode/playhead context; distinguish menu deletion at playhead from selected-keyframe context action. |
| Curves | Chapter 134, pp. 3189–3208 | Graph, parameter fields and modifier gestures; no dedicated local destructive shortcut established. |
| Color Warper | Chapter 136, pp. 3216–3237; point-selection/modifier tools (pp. 3225–3235) | Point selection, fields and viewer interaction. Modifier-click behavior alone is not evidence of application shortcut routing. |
| Qualifier | Chapter 137, pp. 3238–3264; highlight shortcuts (pp. 3251–3252) | Fields and Viewer picker, plus page-level highlight commands. |
| Camera Raw, Color Match, HDR, RGB Mixer | Respective controls in pp. 3121–3188 | Fields and dropdowns, page-level grading commands, modifier gestures. No separate local shortcut set established. |
| Magic Mask | Chapter 139, pp. 3280–3306 | Viewer strokes/clicks and palette controls; establish docked key behavior before assigning expected deletion semantics. |
| Motion Effects, Blur, Key, Sizing, 3D | Controls overview pp. 3055–3058; Key chapter 146, Sizing chapter 152, Motion Effects/Blur chapter 153 | Numeric entry and native tool contexts; no separate local key set established. |
| Info | Clip information display (p. 3059) | No local editing command established; regression-test focus departure and application commands. |

### Fairlight-specific panes

| Pane | Documented behavior and source | Validation target |
| --- | --- | --- |
| Index | Edit/Tracks/Markers tabs (pp. 3786–3788); track rename and selection | Treat tabs and their fields separately; establish native navigation/deletion behavior before testing floating equivalents. |
| Groups | Ctrl+numeric-keypad 0 toggles Groups, 1–9 individual groups, * All Group (p. 3997); rename (p. 3999) | Page commands versus group-name editing and selected tracks. |
| ADR | Character-name typing/Return (p. 3848), cue workflows (chapter 174) | Editable name/cue/time fields versus playback/record commands. No distinct panel-local default shortcut set established. |

Native Edit/Fairlight timelines are essential control cases and possible unintended
recipients even though Unbound does not float them. Media/Cut/Fusion/Deliver are not
current supported pages. Fusion documents the same focus model (p. 1288), but its
Nodes, Spline and Keyframes command coverage belongs to future Fusion support.

## Keyboard Customization inventory

Observed in the running Windows Resolve with the user's existing
`DaVinci Resolve - Wolfgang` preset. Categories were inspected through the UI;
no mappings were edited or saved. These are current-preset observations, not
claims about factory defaults or every available command.

The complete **Panels** category list contains 22 contexts:

| Grouping for this audit | Exact UI category names |
| --- | --- |
| Shared media/editing | Edit/Media Viewer; Media Pool; Sound Library; Edit Timeline; Metadata |
| Gallery | Gallery Memories; Gallery Project Stills; Gallery Media View |
| Color | Color Viewer; Color Nodes; Color Thumbnail Panel; Color Timeline Panel; Color Keyframes |
| Fairlight | Fairlight Timeline; Fairlight ADR |
| Standalone keyframe editor | Keyframe Editor Widget |
| Outside current pane support | Project Manager; Media Storage; Fusion Page; Cut Timeline; Photo Inspector; Photo Page |

Application is a separate command scope. There are no separate Panels categories
for Edit/Fairlight Inspector, Effects, Index, Mixer, Groups, individual lower Color
palettes or Scopes. Absence from this list does not mean those widgets ignore keys.
For example, Color Viewer contains scope-related actions such as Histogram, Parade
and Hide Scopes on Play, with no assignments shown in the inspected list.

### Observed overlapping mappings

| Context | Backspace | Delete |
| --- | --- | --- |
| Application | Delete Selected | Ripple Delete |
| Media Pool | No explicit entry shown | Remove Clips |
| Edit Timeline | Delete Selected | Ripple Delete |
| Gallery Memories / Project Stills / Media View | Delete Selected | Ripple Delete |
| Color Nodes / Thumbnail Panel / Timeline Panel | Delete Selected | Ripple Delete |
| Color Keyframes | No explicit entry shown | Delete Selected Keyframe |
| Keyframe Editor Widget | Delete selected Keyframes | Delete selected Keyframes |
| Cut Timeline | Delete Selected | Ripple Delete |

The Active Key view showed no explicit Delete/Backspace assignment for Project
Manager, Media Storage, Edit/Media Viewer, Sound Library, Metadata, Color Viewer,
Fairlight Timeline, Fairlight ADR, Fusion Page, Photo Inspector or Photo Page.
This does not exclude application fallback or native widget handling.

Additional command-list checks:

- Media Pool **Remove Clips** has Shift+Backspace and Delete. **Remove Bin**,
  **Remove All Clips in Bins...** and **Remove...** show no assigned key. Native
  docked Backspace bin deletion therefore cannot be inferred from a dedicated
  Remove Bin binding. Its precise dispatch path remains unproven.
- Color Nodes **Ripple Delete** has Shift+Backspace and Delete; **Delete Selected**
  has Backspace. Application > Color > Nodes > **Delete Current Node** is a distinct
  command with no assignment shown. Do not equate a customization label with the
  final action performed by a focused node canvas.
- Color Keyframes **Delete Selected Keyframe** has Shift+Backspace and Delete.
- Sound Library's category lists only **Add Library**, unassigned. The L key's
  Active Key view shows Application > **Play Forward**, with no Sound Library entry.
  Correct audition transport therefore also depends on native player targeting,
  beyond the existence of a panel-specific customization entry.
- Edit/Media Viewer includes **Mark In** (I), **Mark Out** (O), **Create Subclip**
  (Alt+B), markers, viewer ganging and recent-clip operations.
- Metadata exposes metadata-group selection commands, such as All Groups, Audio,
  Camera, Clip Details and Shot & Scene; the visible entries are unassigned.
  This category is not an inventory of its text editor's keystrokes.
- Fairlight ADR exposes **ADR Play**, **ADR Record**, **ADR Rehearse** and **ADR Stop**,
  all unassigned. This adds explicit assignable panel commands to the manual audit.

All Commands searches for deletion, tracking and points did not establish an
explicit tracking-point Delete mapping. The tracking warning observed with floating
Nodes cannot yet be attributed to a specific configurable shortcut collision.
The UI inventory explains overlapping contexts, not Resolve's complete dispatch path.

## Required validation and implementation decision

1. Use the inventory above alongside the manual matrix. A category is not equivalent
   to a floating window, and its command list does not include all native field,
   selection or player behavior. Extend individual command coverage where a test
   requires it; preserve the user's preset.
2. For each matrix row, establish a docked baseline, then compare floating behavior.
   Start with Nodes, Media Pool on every supported page, Edit Keyframes, Color
   Timeline markers, viewer tools and Metadata keywords because deletion is contextual.
3. Distinguish OS/Qt active window, focused child, native panel indicator, selected
   content and actual command recipient. Test first activation, another click within
   the already-active host, switching subcontrols, and keyboard-only panel selection.
4. Keep an unrelated disposable selection in another panel/timeline and verify it
   remains untouched. Use disposable content and restore changes; test Undo scope.
5. Test editing text/numbers (Delete/Backspace, clipboard, arrows, Return, Escape),
   popups/dialogs, app return, dock/undock, layout recall and page return. Preserve
   native playback targeting, custom mappings and focus choices.
6. Decide shared bridge scope from this evidence. Native expanded palettes, viewers
   and global Scopes have different lifecycles; do not blanket-send activation events
   or forward keys on the assumption that every host fails identically. Validate on
   Windows and macOS before claiming a general fix.
