# Passive audition activation diagnostic

Open investigation: [floating-pane keyboard routing](../../../../doc/issues/floating-pane-keyboard-routing.md).
Production contracts and thread/ownership rules are in [AGENTS.md](../../../../AGENTS.md).

This diagnostic uses Resolve's existing Qt runtime. Entry queues onto the GUI
thread; one widget inventory identifies native players. It observes focus,
top-level activation, mouse presses, and existing `IsActiveChanged(bool)` and
`MuteAudioChanged(bool)` signals. It does not invoke player actions, change focus,
selection, audio settings or geometry. Observation stops after three minutes or
3,000 qualifying events. Only J/K/L/Space key events qualify; typed text is not
recorded. Logs append locally and include widget identities, not audio samples.

From the repository root:

```sh
cmake -S tests/platforms/macos/audition-audio -B build/macos/audition-audio/native -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/macos/audition-audio/native --parallel
codesign --force --sign - build/macos/audition-audio/native/libaudition-audio.dylib
QT_QPA_PLATFORM=cocoa QT_QPA_PLATFORM_PLUGIN_PATH='/Applications/DaVinci Resolve/DaVinci Resolve.app/Contents/PlugIns/platforms' build/macos/audition-audio/native/signal-test
```

Use a fresh build directory if an older CMake source directory is cached there.
The signal fixture verifies bool argument delivery; it cannot validate Resolve's
audio behavior. Intel execution is not covered by this diagnostic validation.

In Resolve's internal Lua Console, substitute the repository's absolute path:

```lua
assert(loadfile([[/absolute/repo/tests/platforms/macos/audition-audio/trace.lua]]))([[/absolute/repo/build/macos/audition-audio/native/libaudition-audio.dylib]], [[/absolute/repo/build/macos/audition-audio/trace.jsonl]])
```

Close Console. Keep one Sound Library clip selected: audition and stop it, play
and stop Program, click unused Sound Library header space, then L/K. Note whether
transport moves and audio is audible. If silent, click the waveform without
reselecting the clip and repeat L/K. Compare the exact sequence docked and floating;
Source return is a separate check. Report audible results alongside the trace:
signals alone do not establish sound output.

Never overwrite a library loaded in Resolve. For another diagnostic revision,
set `-DUNBOUND_AUDITION_PROBE_NAME=audition-audio-next`, sign that output and pass
its new path to the Lua launcher. Wait for the previous observer to stop before
loading another revision. Re-running the same library replaces its own observer.
The Lua table retains library handles for callback lifetime safety.
