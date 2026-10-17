# Unbound settings

Location: `%LOCALAPPDATA%\Unbound\Unbound.json`.

The file is readable JSON (format version 5). If you want to edit it directly
then do it while Resolve is closed, so Unbound's in-memory settings and
autosave do not overwrite your changes. Settings apply across Resolve projects
for the current Windows user.

The top-level `pages` object uses Resolve widget names: `WStackPage_Conform`, `WStackPage_Color`,
`WStackPage_Audio`, `WStackPage_Browse`, `WStackPage_Cut`,
`WStackPage_Fusion`, and `WStackPage_Deliver` as page names. Entries appear as they are used.
Each page can contain `mainWindow` and `panes`.

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

`mainWindow` contains:

- `normalRect`: native Windows position and size, with `x`, `y`, `width`, `height`.
- `coordinateSpace`: `windows-workspace`; these are Windows placement coordinates,
  which account for the work area and are distinct from Qt client coordinates.
- `maximized`: whether to restore the main window maximized on this page.
- `monitors`: readable objects with `name`, `workArea` (x/y/width/height), and
  `scaleFactor`. This captures the display arrangement used for the placement.
- `qtFallback`: the same readable Qt geometry fields as a pane's `window`, used
  when the monitor arrangement changes. In an unchanged display setup,
  `normalRect` is authoritative. Update the fallback too if changing both cases.

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
