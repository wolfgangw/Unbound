# macOS installer app

Build a self-contained installer from a previously built private-runtime Lua
candidate:

```sh
infrastructure/macos/installer/build.sh /absolute/path/to/release/mac/lua/yyyy.mm.dd.NNN
```

The immutable output is
`release/mac/installer/yyyy.mm.dd.NNN/Unbound <version> (<runtime-build>).app`.
The app name, window title and application menu identify the bundled runtime,
for example `Unbound 0.3.0 (2026.10.18.018)`. The outer directory identifies the
packaging run. Current candidates are internal builds; public releases will use
`Unbound <version>.app` without the internal-build suffix.
This explicit packaging step preserves the input candidate and installed files.
It does not create a ZIP, publish a release or change the public version.
The footer shows the installed version below Uninstall and the bundled version
below Install/Update, aligned left and right respectively. The bundled version and
internal build identifier come from the candidate's `VERSION` and `BUILD` metadata;
installation copies them into the private runtime alongside its libraries.
Older installations without metadata show "Version unknown" until updated;
the installed-version footer is hidden when Unbound is absent. Rebuild older candidates
without that metadata before packaging; the current checkout's version is never
substituted for the bundled runtime's version.
Update is disabled when both installed version and internal build identifier match
the bundled runtime; unknown versions and different builds remain updatable.

Open the app in Finder. It offers **Install/Update** and **Uninstall** for the
current macOS account, and asks the user to close Resolve before making changes.
It never quits Resolve automatically. Installation starts Unbound on the next
normal Resolve launch; no permanent installer service or Resolve menu entry is
created. The app can be run directly from its downloaded location.

Uninstall removes the private runtime, startup hook, legacy Utility-menu files
and Lua installation/startup backups. Settings and layouts are kept by default;
**Also remove settings and layouts when uninstalling** explicitly removes both
current and legacy settings files. Other Resolve scripts and unrelated files
are preserved. Existing diagnostic logs remain.
When only retained settings remain, the checkbox is hidden and the action becomes
**Remove settings and layouts…**, with a separate confirmation. With no installation
or saved settings, Uninstall is disabled and the checkbox stays hidden.

The native AppKit UI lives in `src/platforms/macos/installer/`. It bundles and runs
the existing Lua `install.sh` and `uninstall.sh`; it owns no duplicate installation
implementation. The bundled runtime uses Resolve's Qt, while the installer itself
uses only macOS system frameworks. No development toolchain is needed to use it.

Builds run isolated filesystem tests against the bundled scripts and a separate
Cocoa UI fixture, then verify the app's ad-hoc signature. The release app excludes
test-only switches and snapshot code. These internal builds are not notarized;
a browser-download test in another user account is useful but does not reset this
Mac's Gatekeeper history. See [platform status](../../../doc/platforms/macos.md).
