# Native Edit navigation latency

Intermittent Up/Down cut-navigation stalls occur in native Resolve without Unbound,
reportedly after loading Source in Dual Viewer mode. Public event measurements
show roughly half-second GUI-thread delays on individual presses. Restart clears
the condition; loading Source alone does not reliably reproduce it.

Timeline focus, hiding or clearing Source, and timeline play/stop do not clear an
already affected session. The trigger remains unknown; no production workaround
is justified. Do not intercept shortcuts or change playback/device preferences.

Investigate source-load order, saved source position and media types while keeping
timeline cuts and focus constant. Separate event-loop delays from GPU presentation
and decoding/monitoring initialization. The bounded public-API diagnostic is in
[viewer-probe README](../../tests/platforms/windows/viewer-probe/README.md).
