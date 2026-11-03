# Fairlight page shortcut ambiguity

On a German keyboard, Shift+7 can collide with Resolve's `/` Play Around Current
Selection shortcut. Traces show ambiguous slash delivery on Media/Cut/Edit versus
successful Shift+7 page selection on Color/Deliver, including with main active.
Unbound registers neither shortcut; an uninstalled baseline is still required.

Next: compare native behavior and a user-approved remapping of one shortcut.
Do not intercept global keys. Page-return focus restoration is a separate implemented
mechanism in [AGENTS.md](../../AGENTS.md#activation-and-page-focus).

Use physical input with the passive `tests/platforms/windows/cinema-focus/` probe;
automation's target activation can mask the failure.
