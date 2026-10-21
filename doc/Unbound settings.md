# Unbound settings

Windows location: `%LOCALAPPDATA%\Unbound\Unbound.json`.
macOS location:
`~/Library/Application Support/Unbound/Unbound.json`.
`UNBOUND_SETTINGS_FILE` overrides the path for isolated tests.

The file is readable JSON (format version 7). If you want to edit it directly
then do it while Resolve is closed, so Unbound's in-memory settings and
autosave do not overwrite your changes. Settings apply across Resolve projects
for the current user. The schema and pane catalogue are shared across platforms;
window coordinates still depend on the target desktop and monitor layout.
Both builds use the same settings and layout implementation, including migration,
validation and atomic saving. Platform adapters only choose the default data
directory and capture/restore native window placement; the loading mechanism
does not select a different settings file or format.

The top-level `pages` object uses Resolve widget names: `WStackPage_Conform`, `WStackPage_Color`,
`WStackPage_Audio`, `WStackPage_Browse`, `WStackPage_Cut`,
`WStackPage_Fusion`, and `WStackPage_Deliver` as page names. Entries appear as they are used.
Each page can contain `mainWindow` and `panes`.

Keyboard focus is remembered per page only within the running session. Returning
to a page restores its last focused window/control when still available, otherwise
the main window retains Resolve's native focus choice. This memory is not written
to this file or included in saved layouts.

## Panel configuration layouts

Right-click any Unbound Dock/Undock button, docked or floating, to open the
current page's layout menu, anchored immediately below the button (subject to
screen-edge placement). The menu stays open when you release the opening
right-click. **Save as new Edit layout** (or Color/Fairlight for the current page) creates a layout
from the current page's arrangement. To update
the active layout instead, use the save icon in its row. Save, recall, update and delete keep the menu open, so deleting
a layout and saving its replacement does not require reopening it. An outside
click dismisses the menu; Escape and switching to another application also dismiss it.

Edit, Color and Fairlight each have five slots. Names are generated per page
(`Edit-1`, `Color-1`, `Fairlight-1`, etc.); deletion does not reuse an old number.
When all five slots are occupied, delete one before saving another. Saving an
identical stored configuration reports its existing name without adding a duplicate.

The active layout is highlighted, even after manual changes. When it differs
from its snapshot, a save icon appears immediately left of its trash button.
The button uses a red matching Resolve's playhead. Click it to
update that layout in place. Changes are checked when opening the
menu, including geometry whose autosave is still pending.

**Dock all** appears only while this page has at least one floating panel
(including Edit viewers). It docks this page's floating panels and cancels pending undocking.
It clears the active layout selection, preserves saved layouts and does not
reset Resolve's layout or consume a slot. A fully docked arrangement can also
be saved explicitly as an ordinary layout.

Hold a layout name for one second to edit it inline. A short click still recalls.
The editor receives keyboard focus immediately and selects the full name.
Unmodified Left/Right collapse the selection to its start/end; typing replaces it.
Enter submits the name only, including for an inactive layout; it never recalls
that layout. Escape cancels editing while leaving the popout open. Moving focus
away or dismissing the popout cancels an unsubmitted edit. Names are trimmed,
must not be blank, and may contain up to 128 characters. Duplicate names are
allowed, even for all five slots. Renaming does not change active/dirty state,
geometry or the saved configuration.

The format-7 JSON keys below retain their original spelling for compatibility;
the UI and code call these saved configurations layouts. No migration is needed
for the wording change, and existing names, IDs and snapshots stay intact.

Each page can also contain:

- `presets`: an array of objects with stable `id`, display `name`, and `configuration`. Each configuration
  contains a snapshot of that page's `mainWindow` and `panes`, using the same fields
  as the active layout. Layouts do not contain other layouts.
- `activePreset`: the ID of the last saved or recalled layout. Manual layout changes
  preserve this identity; deleting it or choosing Dock all clears it. It is not
  included in snapshots. Older settings may omit it.
- `nextPresetNumber`: the next generated name number for this page.

Saving flushes pending main/floating-window geometry and captures supported pane
docking and native toggle states. Recall restores main placement, reuses existing
floating hosts where possible, and docks or undocks changed panes through normal
restoration. Other pages and archived layouts are left unchanged.

Clicking the already active, unchanged layout does nothing. If it is dirty,
clicking its name restores the saved snapshot; clicking its save icon updates it.
Recalling another layout discards unsaved changes to the outgoing arrangement.
There are no separate per-layout drafts: its snapshot stays unchanged, and
returning to it recalls that snapshot with no retained dirty status.
The popup retains its rows while switching layouts.
Any interaction outside the popup dismisses it, including scrolling and moving
or resizing a window. Hovering outside leaves it open.

Subsequent manual changes autosave the current arrangement; the saved layout changes
only when its update icon is used. Explicit updates preserve the layout name and slot. Duplicate
suppression applies to saving a new layout, not overwriting an existing one.

Layouts cover Unbound's tracked window/pane state. They do not capture Resolve's
internal panel split positions, timeline contents, or Single/Dual Viewer Mode.
Source floating intent still waits until Source becomes natively available.
Existing minimum sizes and monitor-recovery behavior apply.

Older settings formats (1-6) migrate automatically to version 7 with their current
layouts preserved. Existing version 6 layouts receive UUIDs, and their active
layout name is translated to its ID. Names, snapshots and the generated-name
counter are preserved. Recall/update/delete use IDs exclusively; labels can be
duplicated or changed freely. Migration does not create layouts or force any layout change.
Writes remain atomic. Unsupported/corrupt settings are left unchanged; malformed
layout records are rejected rather than recalled as empty configurations.

## Active layout

For example, `pages.WStackPage_Conform.panes.frameEditPageMediaPool` contains:

```json
{
    "floating": true,
    "toolbarButtonChecked": false,
    "window": {
        "clientRect": { "x": 20, "y": 51, "width": 434, "height": 600 },
        "normalClientRect": { "x": 20, "y": 51, "width": 434, "height": 600 },
        "frameRect": { "x": 19, "y": 20, "width": 436, "height": 632 },
        "maximized": false,
        "fullScreen": false,
        "screenIndex": 0,
        "screenWidth": 3440
    }
}
```

- `floating`: restore this pane as a floating window when its page is active.
- `toolbarButtonChecked`: desired state of its native Resolve toolbar button.
  False allows the native main-window area to collapse while the pane floats.
  Manual undocking turns this toggle off automatically; docking turns it on.
  Startup restoration honors the saved choice after initializing native content.
- `clientRect`: position and size of the window's content, excluding its resize
  borders. Floating panes have no Windows title bar. This is the rectangle used
  to restore a normal floating window; existing saved client sizes remain valid.
- `normalClientRect`: the unmaximized rectangle used for maximized/fullscreen
  restoration. When editing an ordinary window, update it alongside `clientRect`.
- `frameRect`: recorded outer Qt frame; retained for Qt's geometry record.
- `maximized` / `fullScreen`: window state. Minimized state is not persisted.
- `screenIndex` / `screenWidth`: Qt monitor-selection and scaling information.
  Coordinates above are Qt logical desktop coordinates; widths/heights must be
  positive integers. Negative x/y values are valid on multi-monitor desktops.

Supported Edit pane keys are `frameEditPageMediaPool`, `frameEditPageSoundLibrary`,
`frameEditPageKeyframeEditor`, `frameEditPageEffectsLibrary`, `frameEditPageEditIndex`,
`frameEditPageInspector`, `frameEditPageMetadata`, and `frameEditAudioMeter`.
Edit viewers use `widgetEditTimelinePlayer` (Program) and
`stackedEditSourceOfflinePlayers` (the complete Source viewer stack). Each stores
independent `window` geometry and `floating` state in the same settings file.
Moving/resizing autosaves after a short delay; docking and shutdown flush geometry.
Shutdown preserves floating intent, while clicking Dock or closing a host clears
it. Restoration waits for Edit to become active; Source waits until available in
native Dual Viewer Mode. Viewer restoration does not force a native mode change.
Single/Dual Viewer switching continues through Resolve's Program header button;
the timeline reclaims viewer space when Program floats and no visible viewer
remains docked. Viewer state has no experiment switch or additional control panel.
Color pane keys are `m_pFrameNodeGraph`, `m_pWidgetGallery`, `m_pWidgetLutBrowser`,
and `m_pWidgetMediaPool`. Color's shared sidebar selectors are exclusive;
save at most one of Gallery/LUTs/Media Pool as checked. All can be unchecked.

Fairlight pane keys under `pages.WStackPage_Audio.panes` are
`frameAudioPanelMediaPoolContainer`, `frameAudioPanelEffectsLibraryContainer`,
`frameAudioPanelIndexContainer`, `frameAudioPanelGroupsContainer`,
`frameAudioPanelSoundLibraryContainer`, `frameAudioPanelAdrContainer`,
`frameTimelineMixerContainer`, `frameMetadataContainer`, and `frameInspectorContainer`.
Meters is excluded. Fairlight uses the same geometry, docking and native toggle
fields as Edit and Color, with restoration deferred until the page is active.
Floating Media Pool audition uses an automatic activation correction with no
additional setting or diagnostic switch. Saved toggle preferences and space
reclamation behave normally; docking removes the correction.

On Windows, `mainWindow` contains:

- `normalRect`: native Windows position and size, with `x`, `y`, `width`, `height`.
- `coordinateSpace`: `windows-workspace`; these are Windows placement coordinates,
  which account for the work area and are distinct from Qt client coordinates.
- `maximized`: whether to restore the main window maximized on this page.
- `monitors`: readable objects with `name`, `workArea` (x/y/width/height), and
  `scaleFactor`. This captures the display arrangement used for the placement.
- `qtFallback`: the same readable Qt geometry fields as a pane's `window`, used
  when the monitor arrangement changes. In an unchanged display setup,
  `normalRect` is authoritative. Update the fallback too if changing both cases.

On macOS, `mainWindow` uses the same readable geometry fields as a pane's `window`:
`clientRect`, `frameRect`, `normalClientRect`, `screenIndex`, `screenWidth`,
`maximized` and `fullScreen`. Rectangles use logical screen coordinates with a
top-left origin; the Cocoa adapter converts to/from native points and handles the
title area. `normalClientRect` preserves the size/position beneath maximization.
Page switching, startup and layout recall all use this native adapter, with shared
off-screen recovery and content constraints. Existing records need no migration.

No field contains Base64 data. Qt's binary restoration record is reconstructed
in memory. This preserves the existing restoration behavior while allowing
direct edits. Content minimum sizes and off-screen recovery still apply.

The floating Edit Inspector excludes its native middle-eliding title label's
horizontal size hint from the window minimum. Other content minimums remain in
effect. Docking restores the label's original sizing policy; no setting or
filename modification is involved.

Pane discovery and deferred restoration run on widget/page lifecycle events;
there is no recurring discovery timer or settings change needed to enable this.
Normal releases have no diagnostic switches. The separate diagnostic build is
documented in tests/platforms/windows/diagnostic/README.md and uses the same settings format.
