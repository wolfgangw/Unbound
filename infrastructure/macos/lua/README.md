# Internal Lua loading

This approach starts the shared controller and Cocoa backend automatically through
Resolve's internal Lua runtime. There is no Scripts-menu entry or manual activation
step. See [platform status](../../../doc/platforms/macos.md) for compatibility and
validation limits.

For graphical installation and removal, build the
[self-contained installer app](../installer/README.md). Developer shell commands
remain available. Build, then install with Resolve closed:

```sh
infrastructure/macos/lua/build.sh
infrastructure/macos/lua/install.sh /absolute/path/to/release/mac/lua/yyyy.mm.dd.NNN
```

Open Resolve normally. The installer places:

- `runtime/` at `~/Library/Application Support/Unbound/Runtime/`, containing
  `load.lua`, `UnboundLuaLoader.dylib` and `UnboundHelper.dylib`.
- `startup/Unbound.scriptlib` at
  `~/Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts/Unbound.scriptlib`.

The hook runs the private loader once and returns. Successful startup is silent;
there is no persistent Lua loop or additional UI. The native loader validates Qt,
retains its libraries, queues attachment on the GUI thread and prevents duplicates.
Diagnostics: `~/Library/Logs/Unbound/lua-bootstrap.log`.

The same installer handles updates and migration from the earlier Utility-menu
layout. It verifies and stages the candidate before moving existing files, backs
up the previous runtime/hook and legacy `Utility/Unbound.lua` / `Utility/UnboundLua`
under `~/Library/Application Support/Unbound/InstallationBackups/`, and restores
previous files if publishing fails. Backups stay outside Resolve's Scripts tree.
Settings are not part of the installation transaction.

Uninstall with Resolve closed using `infrastructure/macos/lua/uninstall.sh`.
Settings and layouts are always retained; there is no option to delete them.
Removal also clears Lua installation/startup backups and legacy menu files; it
preserves other scripts and diagnostic logs.

For development, [disable automatic activation](startup/README.md) when a native
Resolve session without Unbound is needed. Already loaded libraries stay in Resolve until it exits. To recall a
candidate using this layout, close Resolve normally and run the installer with
that candidate. Historical menu-based candidates retain their original layout
and must be restored from their matching backup if needed.

Candidates are immutable; builds do not install, create ZIPs or change public
versions/tags. `UNBOUND_USER_HOME` redirects installer paths for isolated fixtures
without changing `HOME`; normal installation uses the current user's home directory.
The read-only console probe remains under `probe.sh` for developer investigation.
