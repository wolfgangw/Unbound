# Installer validation

`infrastructure/macos/installer/build.sh <absolute-Lua-candidate>` tests the exact
bundled shell scripts using `../loading/lua/install_test.py`. The fixtures use
isolated user directories, mocked process/signature checks and injected failures;
they cover install, legacy migration, update, rollback, uninstall, settings
preservation/removal and unrelated-file protection.

`smoketest.h` is included only in a separate build of the actual AppKit controller.
It opens its own window, verifies the Resolve guard, settings checkbox default and
bundled resources, and writes a window snapshot under `build/macos/installer/`.
Isolated runtime metadata checks fresh/unknown installations, matching builds and
different versions/builds: only matching installed builds disable Update.
The UI fixture also checks installed, settings-only (current and legacy) and empty
states, including checkbox visibility, removal actions and the Resolve guard.
Shell fixtures verify removal of retained settings after uninstalling the runtime.
It never invokes installation/uninstallation or touches Resolve. The distributable
app has no test-only command-line behavior.

Manual playthrough: install in a fresh account; launch Resolve and check automatic
activation; close Resolve and update; uninstall with settings retained, then
reinstall and check layout recovery; finally test explicit settings removal.
Download through a browser when checking Gatekeeper behavior. Another account on
the development Mac does not provide a clean machine-level security state.
