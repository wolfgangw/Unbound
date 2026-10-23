# Internal divider positions in saved layouts

Status: proposed, not implemented. Current layouts save window geometry, pane
floating state and native visibility toggles, not internal split positions.

The desired extension is to preserve useful panel proportions when recalling a
layout. Floating Color Clips already captures native sidebar widths within the
session and uses a temporary normal-Color-mode gesture to let Resolve calculate
new widths. That gesture mechanism is documented in AGENTS.md; it is not yet a
safe programmatic restore interface for saved divider positions.

Before implementation, establish a reversible native-state restoration path,
choose widths versus relative proportions and monitor-size behavior, and define
migration/absence semantics. Validate normal Color and floating Clips layouts,
native constraints, missing/hidden panels and both backends. Do not use private
object offsets or assume setting visible widget geometry updates native anchors.
