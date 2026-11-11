# Timeline-switching stalls on macOS

Open report: [GitHub issue #4](https://github.com/wolfgangw/Unbound/issues/4).

The reporter observes 1–3 second UI stalls when switching timelines, including
four empty timelines in a fresh project, on macOS Resolve Studio 21.2 with Unbound
0.7.0. Sampled stacks implicate `WindowGroupActivation::refreshLevels()` during
menu destruction; disabling Unbound removes the stall.

## Windows baseline

User-confirmed: **no freezes** with Unbound internal build `2026.11.11.002`
(0.7.0) and Resolve Studio `21.2.0.5` on Windows.

Reproduction project: **Unbound Issue #4**, containing four empty timelines named
Timeline 1–4, with no media or effects. On Edit, Display Stacked Timelines is
enabled and all four timelines are open as tabs. The comparison procedure is to
switch through the tabs and their dropdowns, then use the timeline selector on
Color, Fairlight and Deliver. This is a qualitative user observation, not a
measured latency benchmark or an installed/uninstalled A/B comparison.

## Remaining investigation

Commit `6db5ed6` narrows Cocoa level refreshes for dynamic property changes to
Unbound's pinning/tearoff properties on group members. It fixes the observed
macOS effect-selection stalls, but does not narrow Show, Hide, Destroy and other
window lifecycle triggers: these still accept unrelated top-level widgets.

Investigate those triggers on macOS before claiming issue #4 is fixed. Preserve
Always on top, native popup/modal priority, application departure and native
window recreation. The Windows baseline does not validate the Cocoa path.
