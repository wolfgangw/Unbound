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
packaging run. Public candidates created with `build.sh --release` produce
`Unbound <version>.app` without the internal-build suffix. Internal runtime and
packaging identifiers both use `yyyy.mm.dd.NNN`, the same convention as Windows.
The bundle's `UnboundPackageBuild` and the packaging directory's BUILD record
identify packaging separately; CFBundleVersion is Apple-specific numeric metadata.
The installer and both runtime libraries are universal by default. Native loading
selects the slice matching the running Resolve process; the Lua script and
installation paths are identical on Intel and Apple Silicon. For a single-slice
development candidate, pass the same `UNBOUND_MAC_ARCHS` to both build commands.
Packaging verifies every payload slice and runs the UI fixture for each runnable
architecture. `UNBOUND_MAC_ALLOW_UNTESTED=1` permits an unavailable execution
architecture with an explicit NOT RUN record, never a false test pass.
This explicit packaging step preserves the input candidate and installed files.
It does not create a ZIP, publish a release or change the public version.

For public distribution, copy the validated `Unbound <version>.app` from the
packaging output into `release/packages/` and explicitly create
`release/packages/Unbound-<version>-macos-universal.zip`. Match the existing package
layout: the app is at the ZIP root. Use `ditto -c -k --sequesterRsrc --keepParent`
with the app as input and the ZIP as output; refuse existing destination artifacts.
Extract the ZIP into a temporary build directory and verify its signature,
byte-identical payload, public version and arm64/x86_64 slices. Keep staging and
validation records under `release/mac/installer/`; internal builds are unchanged.

Layout, labels and feedback come from the [shared installer definition](../../installer/README.md),
also used by Windows. The installed version is the second line inside Uninstall
on the left; the candidate version is inside Install/Update on the right. Internal
versions display `yyyy.mm.dd.NNN`, without a prefix. The status line is at the
bottom beneath a thin divider.
The bundled version and internal build identifier come from the candidate's `VERSION` and `BUILD` metadata;
installation copies them into the private runtime alongside its libraries.
Older installations without metadata show "Version unknown" until updated;
the installed-version line is omitted when Unbound is absent. Rebuild older candidates
without that metadata before packaging; the current checkout's version is never
substituted for the bundled runtime's version.
Update is disabled only when the installed VERSION, BUILD, loader script, both
native libraries and startup hook match the bundled payload byte-for-byte.
Missing, unreadable or changed files remain updatable, including installations
whose version metadata still matches, so Update can repair them.

Open the app in Finder. It offers **Install/Update** and **Uninstall** for the
current macOS account, and asks the user to close Resolve before making changes.
It never quits Resolve automatically. Installation starts Unbound on the next
normal Resolve launch; no permanent installer service or Resolve menu entry is
created. The app can be run directly from its downloaded location.

Uninstall removes the private runtime, startup hook, legacy Utility-menu files
and Lua installation/startup backups. Settings and layouts are always kept,
including legacy settings files. Other Resolve scripts, unrelated files and
diagnostic logs are preserved. There is no settings-deletion action.
With no installation, Uninstall is disabled even when saved settings remain.

The native AppKit UI lives in `src/platforms/macos/installer/`. It bundles and runs
the existing Lua `install.sh` and `uninstall.sh`; it owns no duplicate installation
implementation. The bundled runtime uses Resolve's Qt, while the installer itself
uses only macOS system frameworks. No development toolchain is needed to use it.

Builds run isolated filesystem tests against the bundled scripts and a separate
Cocoa UI fixture, then verify the app's ad-hoc signature. The release app excludes
test-only switches and snapshot code. Builds are ad-hoc signed, without Developer ID
signing or notarization; see the [README](../../../README.md#compatibility) for
the per-app Open Anyway procedure. These distribution limits do not affect
feature parity with Windows. A browser-download test in another user account
is useful but does not reset this
Mac's Gatekeeper history. See [platform status](../../../AGENTS.md#platform-implementation-and-validation).
