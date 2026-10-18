# Shared installer presentation

`src/installer/dialog.json` is the source of truth for installer wording, fonts,
dialog size and control rectangles. Coordinates are logical units measured from
the top left. `generate.py` validates it and emits a C++ header into each build
directory. The Windows CMake build and macOS installer script both generate this
header; C++ and Objective-C++ consume it directly. Python 3 is a build dependency
only. No JSON file or interpreter is required by the installed app.

`src/installer/presentation.h` supplies shared action labels, visibility/enabled
states, confirmations, feedback and display-version formatting. Native backends
provide installation/process state and draw native controls. Keep filesystem
transactions, process monitoring and platform paths in their existing backends.

The logo is optically centered against the complete two-line text block (app name
and subtitle together), not against the app name alone. Its 48-unit artwork box
starts at y=32, with the title at y=22 and subtitle at y=62. This accounts for the
artwork's transparent padding and the visible text within the native label frames.
Keep this relationship in `dialog.json`, shared by both renderers; do not add
platform-local positioning offsets.

The header is followed by equal-size two-line action buttons:
Uninstall on the left and Install/Update on the
right. The installed and candidate versions are inside their respective buttons,
without prefixes. Internal versions display `yyyy.mm.dd.NNN`, with the counter
padded to at least three digits; public versions display their semantic version.
Windows semantic internal metadata and macOS's separate BUILD field normalize to
the same display value. Missing installed metadata displays Version unknown.
Uninstall always preserves settings and layouts; there is no deletion action.
Without an installed runtime/startup hook, Uninstall is disabled, even when settings remain.
The status line is at the bottom,
separated by a thin rule. Native fonts, borders and disabled-control rendering
follow the OS; geometry and content are shared.

Windows installer state tests exercise the real native controls, row order,
multiline labels, retained-settings state and the shared state/version functions.
The macOS smoke fixture consumes the same definition and checks the corresponding
frames and labels. Run it on a Mac to validate AppKit rendering; Windows cannot
compile or visually validate that backend.
