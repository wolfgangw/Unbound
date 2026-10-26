# Lower Color controls: remaining validation

Current mechanisms and supported tools are documented in AGENTS.md. General
floating-control use is user-validated on Windows.

Remaining checks:

- Validate macOS, including native graphics surfaces.
- Validate 3D with stereoscopic material; it is disabled in the current Windows
  test project. Include title/background dragging and native selector clicks.
- Check alternate menu/shortcut routes to native expansion. The recognized
  Expand buttons are guarded while floating.
- Compare Scopes' empty plot immediately after a mode change against native
  docked behavior. A new frame/clip update supplies data while floating.

The original tool slots retain placeholders while their controls float. Lower
region reclamation and automatic selection of a replacement docked tool remain
unsettled; viewer reclamation is tracked separately in color-viewer.md.

## Curves and Color Warper native-expansion validation

The shared implementation retains Resolve's registered expanded dialog inside the
standard Unbound host; the mechanism is documented in AGENTS.md.

Windows user testing confirms both floating palettes update and dock correctly.
Key's matte updates work with its existing implementation. Remaining coverage:

- Curves and Warper floating together, including alternate modes and clip/node changes.
- Page/layout switches, geometry recall and restart restoration.
- Native replacement-tool selection.
- Native menu/shortcut expansion and return paths beyond the guarded buttons.
- macOS native expansion, ownership and graphics-surface behavior.

Fixtures cover native-dialog retention/return, page hiding, already-expanded
adoption, native Close/Escape and rejected-close recovery. They do not replace
validation in Resolve.

## Active-mode sizing validation

The shared Warper sizing mechanism is documented in AGENTS.md. Validate in Resolve:

- Switching among Chroma Warp, Hue/Saturation and Chroma/Luma grows the host when
  required, preserves controls, and permits shrinking again after returning.
- Both Curves and Warper still update together after playback stops, including
  page/layout changes, docking/refloating and restart restoration.
- macOS behavior, native rendering and mode-dependent minimums.

Fixtures cover mode selection, late/removed pages, active control minimums,
policy restoration, native registration and rejected-close recovery. Optional
public Qt size captures are described in
[the Windows infrastructure README](../../infrastructure/windows/README.md).
