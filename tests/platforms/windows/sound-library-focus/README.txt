Sound Library shortcut routing diagnostic (GitHub issue #2)
=========================================================
Build from the repository root:
  cmake -S tests/platforms/windows/sound-library-focus -B build/windows/sound-library-focus -A x64 -DCMAKE_PREFIX_PATH="%USERPROFILE%/Qt/5.15.2/msvc2019_64"
  cmake --build build/windows/sound-library-focus --config Release
In PowerShell use $env:USERPROFILE rather than %USERPROFILE%.

Run build/windows/sound-library-focus/Release/sound-library-focus.exe with:
  --passive  Observe docked/floating activation, focus, shortcuts and player signals.
  --bridge   Replace that capture with the experimental activation bridge.
  --stop     Stop recording and disable the bridge.

Use the current Resolve session; no restart is needed. Start with a selected
Sound Library result on Edit. Compare Space, J/K/L and I/O after clicking the
result/player while docked, then while floating. Confirm actual player behavior,
not just focus logs. Also compare timeline handoff and docking back.

The bridge variant reuses audition-events/bridge.h, restricted to the Edit
Sound Library host and m_PlayerPreview. It sends a main-window activation
notification only on genuine floating-host activation, consuming only that
synthetic event before QWidget propagation. It does not force keyboard focus,
forward keys, call playback methods or access private memory. This is an
unvalidated diagnostic hypothesis, not a production fix.

One public Qt widget inventory runs on explicit start. Records go to adjacent
logs/*.jsonl; each capture stops after five minutes or 2500 records. --stop
disables intervention but does not force native context restoration; clicking
main lets Resolve perform its normal activation handling. Loaded DLL remains
until Resolve exits. Do not rebuild/replace it while loaded.

No production source, settings or artwork changes. A fresh Resolve process is
required for any later production helper validation. macOS has not been tested.
