# Lower Color controls: remaining work

Supported controls, native expansion, active-mode sizing and validated Windows
startup restoration are documented in [AGENTS.md](../../AGENTS.md).

- Validate macOS ownership, native graphics surfaces, sizing and page/layout/startup
  restoration, including Curves and Warper updating simultaneously.
- Broaden Windows coverage for Warper modes, clip/node changes and page/layout
  combinations. Larger modes must retain native minimums and allow later manual
  shrinking when the active mode permits it.
- Broaden main-window resize preservation checks to explicit recalls, manual
  floating resizes and other pages; basic Color startup, maximize/restore and page
  return are validated. Qualifier HSL compact sizing is user-validated; check larger
  modes as well.
- Validate 3D with stereoscopic material, including native controls and dragging.
- Check alternate native expansion/return routes and replacement-tool selection.
- Validate native multi-scope ownership, independent updates, sizing, view-count
  controls and docking on macOS. Windows behavior is documented in AGENTS.md.

Lower-region reclamation and replacement-tool selection remain unsettled.
Viewer reclamation is tracked in [color-viewer.md](color-viewer.md). Fixtures cover
ownership, constraints and cancellation but do not replace native Resolve checks.
Public Qt size/restore diagnostics are documented in the
[viewer-probe README](../../tests/platforms/windows/viewer-probe/README.md).
