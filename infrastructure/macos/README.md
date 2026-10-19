# macOS infrastructure

- `prepare-qt.sh`: fetch pinned Qt 5.15.2 source and configure development headers.
- `resolve-qt.cmake`: import Resolve's Qt runtime; no replacement Qt is shipped.
- `test.sh [build-dir]`: run the Lua loader and Cocoa/shared fixtures.
- `audit.sh <candidate-directory>`: check architecture and allowed dependencies.
- `build.sh [--release]`: build, test, audit, ad-hoc sign and retain an immutable
  universal Lua candidate. `--release` explicitly uses the current VERSION.
- `install.sh <absolute-candidate-directory>`: install/update with Resolve closed.
- `installer/build.sh <absolute-Lua-candidate>`: explicitly package and test a
  self-contained installer app with install/update/uninstall actions.
- `lua/probe.sh`: build a read-only native-loading probe for Resolve's Lua console.
- `lua/build.sh`: build/test/audit/sign a Lua candidate under
  `release/mac/lua/yyyy.mm.dd.NNN/`, using `build/macos/lua/<variant>/`.
- `lua/install.sh <absolute-candidate-directory>`: install/update the private
  runtime and automatic startup hook with Resolve closed; migrate legacy menu
  files into backups and preserve settings. No Scripts-menu entry is installed.
- `lua/startup/enable.sh <absolute-candidate-directory>`: developer alias for
  normal installation, restoring automatic startup with a matching runtime.
- `lua/startup/disable.sh`: move only the startup hook outside Resolve's Scripts
  tree; retain the runtime and settings for isolated development tests.

The top-level `build.sh`, `test.sh`, `audit.sh` and `install.sh` delegate to Lua.
`UNBOUND_MAC_ARCHS` selects `universal` (default), `arm64` or `x86_64`.
Build output lives under `build/macos/lua/<variant>/`; candidates remain under
`release/mac/lua/`. Public candidates use `<VERSION>/` and contain both architectures.
The same source files and Qt headers compile for each target; Qt's processor
detection follows the compiler target. Resolve's linked libraries must supply
every requested slice; CMake checks this before building. No second Qt is shipped.
Tests select each executable architecture explicitly. When a host cannot execute
a slice, builds stop unless `UNBOUND_MAC_ALLOW_UNTESTED=1` is set. That exception
is recorded as NOT RUN in validation logs and never suppresses a failing test.
See [Lua instructions](lua/README.md).

These workflows are separate from the Windows release entry points. See
[platform status](../../doc/platforms/macos.md) for requirements and validation.
