# SPDX-License-Identifier: MIT
"""Summarize a buffered recall trace. Inclusive spans must not be added together."""
import collections
import json
import sys
from pathlib import Path
for argument in sys.argv[1:]:
    path = Path(argument)
    data = json.loads(path.read_text(encoding="utf-8"))
    events = data["events"]
    spans = collections.defaultdict(list)
    for row in events:
        if "duration_us" in row:
            spans[row["phase"]].append(row["duration_us"] / 1000)
    print(path.name, "page:", data["page"], "dropped:", data["dropped_events"])
    if "finish_reason" in data:
        print(f"  observation: {data['finish_reason']} at {data['elapsed_us']/1000:.2f}ms; pending={data['pending_at_end']}; structural_events={data['structural_events']}")
    if "adapter_pending_at_end" in data:
        print(f"  adapter work remaining: {data['adapter_pending_at_end']}")
    for phase, samples in sorted(spans.items()):
        print(f"  {phase}: n={len(samples)} inclusive_total={sum(samples):.2f}ms max={max(samples):.2f}ms")
    for phase in ("menu_finish_recall", "clean_recall", "quiet_candidate_not_gpu_ready", "work_and_geometry_quiet", "quiet_blocked_by_adapter_work", "all_visible_gl_windows_composed_not_global_ready", "candidate_invalidated", "candidate_invalidated_by_work", "palette_size_reapply", "docking_check_passed", "public_gl_surface", "final_update_requested", "final_update_attempt_limit", "post_update_candidate_not_native_ready", "cover_visible", "cover_reveal_candidate", "cover_candidate_invalidated"):
        matches = [row for row in events if row["phase"] == phase]
        if matches:
            print(f"  {phase}: n={len(matches)} first={matches[0]['us']/1000:.2f}ms last={matches[-1]['us']/1000:.2f}ms")
    for row in events:
        if row["phase"] in ("cover_end", "cover_bitmap_bytes", "page_capture_departure", "page_capture_arrival", "page_destination", "page_incoming_show", "page_current_changed", "page_precapture_paint_deliveries", "page_precapture_update_requests"):
            print(f"  {row['phase']} at {row['us']/1000:.2f}ms: {row.get('pane', '')}")
        if row["phase"] == "composition_evidence":
            print(f"  composition at {row['us']/1000:.2f}ms: GL waiting={row['gl_waiting']}; raster unproven={row['raster_unproven']}")
    for row in sorted((row for row in events if "duration_us" in row), key=lambda row: row["duration_us"], reverse=True)[:12]:
        print(f"  slow span: {row['phase']} {row.get('pane', '')} {row['duration_us']/1000:.2f}ms ending at {row['us']/1000:.2f}ms")
    pending = [row for row in events if row["phase"] == "controller_pending"]
    for row in pending:
        print(f"  pending at {row['us']/1000:.2f}ms: discovery={row['discovery']} initialization={row['initialization']} toggles={row['toggles']} floating={row['floating']}")
    surfaces = {row["id"]: row for row in data.get("gl_surfaces", [])}
    for name, value in data["paint_summary"].items():
        surface = surfaces.get(name.removesuffix(":frameSwapped"))
        label = f" [{surface['class']} originally in {surface['initial_window']}]" if surface else ""
        print(f"  {name}{label}: count={value['count']} first={value['first_us']/1000:.2f}ms max_observed_gap={value['max_gap_us']/1000:.2f}ms ending={value.get('max_gap_end_us', 0)/1000:.2f}ms")
