# Issues and investigations

User reports, reproduction evidence and follow-up investigations live here.
Distinguish confirmed findings from hypotheses; historical investigation records
may describe intermediate experiments rather than the current implementation.

- [Floating panels after leaving Dual Screen mode](dual-screen-floating-panels.md):
  unconfirmed Resolve report; a matching lifecycle weakness reproduced in a fixture.
- [Edit Media Pool collapsed to one pixel](edit-media-pool-collapsed-width.md):
  native failure when entering optimized vertical-video layouts with Media Pool
  hidden; project-resolution changes also reproduce disappearance on docking.
  Native recovery and Unbound's defensive docking safeguard are validated in Resolve.
- [Fairlight page focus and shortcuts](fairlight-page-focus-and-shortcuts.md):
  returning floating windows take transport focus; a separate native Shift+7/slash
  shortcut collision depends on the originating page.
- [Fairlight Media Pool audition](fairlight-audition-investigation-2026-10-13.txt):
  investigation of lost audio and playback handoff while floating.
- [Layout rename input](layout-rename-input-2026-10-17.txt):
  keyboard routing during inline name editing.
- [Corner resize crash](resize-corner-crash-2026-10-17.txt):
  crash evidence, root cause and correction.
