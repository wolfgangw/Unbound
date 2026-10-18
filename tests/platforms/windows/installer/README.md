# Windows installer tests

`UnboundInstallerTests` exercises the production transaction implementation in
temporary directories. It covers fresh install, update, rollback at each stage,
Resolve running before/during preparation, incomplete-install repair, uninstall,
retained current/legacy settings, reinstall, repeated uninstall and
unrelated-script preservation. It never operates on the user's installation.

Installer builds also run `UnboundInstallerStateTests`, which drives the actual
window procedure and background monitor with simulated process presence. Without any
activation messages it checks quit/start transitions, matching-version Update
state and preservation of operation feedback. Its hidden window and temporary
installation are isolated from Resolve and user settings. It also verifies
background/idle priority and that 1,000 checks of a running fixture process use
one process-list snapshot and then only its cached synchronization handle.

The native graphical installer embeds the tested runtime, startup hook and
version metadata. Inspect Install/Update and Uninstall states with Resolve open
and closed. Matching complete installations disable Update; missing/changed
files allow repair. Uninstall always keeps settings. Retained settings alone
must not enable Uninstall. Check normal startup and layout recovery after reinstall.
