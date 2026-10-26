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
