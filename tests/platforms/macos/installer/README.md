# Installer validation

`infrastructure/macos/installer/build.sh <absolute-Lua-candidate>` tests the exact
bundled shell scripts using `../loading/lua/install_test.py`. The fixtures use
isolated user directories, mocked process/signature checks and injected failures;
they cover install, legacy migration, update, rollback, uninstall, settings
preservation and unrelated-file protection.

`smoketest.h` is included only in a separate build of the actual AppKit controller.
It opens its own window, verifies the Resolve guard, action availability and
bundled resources, and writes a window snapshot under `build/macos/installer/`.
Isolated payload checks cover fresh/unknown installations and different
versions/builds. Only a complete byte-for-byte match disables Update. Each runtime
file and the startup hook is independently removed, restored and modified without
changing its length; the UI must allow repair, then disable Update after restoration.
The Resolve-running guard still disables repair actions. An orphaned startup hook
also remains removable/updatable.
The UI fixture also checks installed, retained-settings (current and legacy) and empty
states. Only an installation enables Uninstall; there is no settings-deletion action.
Shell fixtures verify that repeated uninstall preserves current and legacy settings
and rejects the obsolete settings-removal option.
It never invokes installation/uninstallation or touches Resolve. The distributable
app has no test-only command-line behavior.

Manual playthrough: install in a fresh account; launch Resolve and check automatic
activation; close Resolve and update; uninstall with settings retained, then
reinstall and check layout recovery.
Download through a browser when checking Gatekeeper behavior. Another account on
the development Mac does not provide a clean machine-level security state.
