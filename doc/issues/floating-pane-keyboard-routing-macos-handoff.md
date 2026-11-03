# macOS keyboard-routing validation

Build/install the shared fix in a fresh Resolve process. Read the
[current issue and coverage](floating-pane-keyboard-routing.md) and
[mechanism](../../AGENTS.md#pane-command-activation-and-audition-players).

Remaining sign-off:

- Edit and Fairlight Sound Library: play Program/timeline, return by header and
  application switching, then J/K/L. Repeat with result-list and search-field focus
  remembered. Confirm audible audition, correct transport target, intact search
  editing/list navigation, and no waveform click or clip reselection required.
- Floating Color Nodes: tear off, then add/delete a disposable node without another
  canvas click. Verify the unrelated original node remains.

For broader coverage, use the issue's Windows lifecycle matrix as a checklist.
Preserve mappings, preferences and layouts; clean up disposable test content.
Ask the user to perform keyboard-only return if automation activates targets.

Cinema return already passes the tested macOS Program-floating/Source-docked cases.
Do not copy the Windows main-restoration workaround without a native failure.
Dual Screen repetition remains pending: Cinema exit and application return directly
to Program, then J/K/L without a click, must target Program/timeline rather than Source.
