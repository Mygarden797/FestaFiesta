#!/usr/bin/env python3
"""Summarize a UE 5.7 CSV profiler capture without third-party packages.

UE CSV files can append columns during capture and repeat the complete header at
the end. Column names can also occur more than once. This reader keeps column
indices, uses the final header when present, and treats absent optional cells
in earlier rows as unavailable rather than shifting their values.
"""

from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import json
import math
import pathlib
import statistics
import sys
from typing import Any


TIMING_COLUMNS = {
    "game_thread": "GameThreadTime",
    "render_thread": "RenderThreadTime",
    "gpu": "GPUTime",
}
CROWD_COLUMNS = {
    "active_agents": "FFCrowd/ActiveAgents",
    "bound_actors": "FFCrowd/BoundActors",
}
MEMORY_COLUMNS = {
    "physical_used_mb": "PhysicalUsedMB",
    "gpu_local_used_mb": "GPUMem/LocalUsedMB",
}


class CsvAnalysisError(Exception):
    """The capture cannot support a trustworthy analysis."""


def is_header(row: list[str]) -> bool:
    return bool(row) and row[0] == "EVENTS" and "FrameTime" in row


def is_metadata(row: list[str]) -> bool:
    return bool(row) and row[0] == "[HasHeaderRowAtEnd]"


def percentile_nearest_rank(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    return ordered[max(0, math.ceil(fraction * len(ordered)) - 1)]


def number_from_indices(
    row: list[str], indices: list[int], *, positive: bool = False
) -> tuple[float | None, int | None]:
    """Use the first finite value in a duplicate-name group, preserving its index."""
    for index in indices:
        if index >= len(row) or not row[index].strip():
            continue
        try:
            value = float(row[index])
        except ValueError:
            continue
        if math.isfinite(value) and (value > 0 if positive else value >= 0):
            return value, index
    return None, None


def scan_header(path: pathlib.Path) -> tuple[list[str], dict[str, str], dict[str, Any]]:
    first_header: list[str] | None = None
    final_header: list[str] | None = None
    metadata: dict[str, str] = {}
    data_rows = 0
    max_data_width = 0
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.reader(stream):
            if is_header(row):
                if first_header is None:
                    first_header = row
                else:
                    final_header = row
            elif is_metadata(row):
                # UE's trailing command line is not CSV-quoted: -trace=a,b,c
                # expands to multiple cells. Rejoin cells until the next
                # bracketed key rather than assuming every key/value is a pair.
                keys = [index for index, cell in enumerate(row)
                        if cell.startswith("[") and cell.endswith("]")]
                for position, offset in enumerate(keys):
                    following = keys[position + 1] if position + 1 < len(keys) else len(row)
                    key = row[offset][1:-1].lower()
                    if key != "loginid":
                        metadata[key] = ",".join(row[offset + 1:following])
            elif row and any(cell.strip() for cell in row):
                data_rows += 1
                max_data_width = max(max_data_width, len(row))
    if first_header is None:
        raise CsvAnalysisError("UE CSV header with FrameTime was not found")
    header = final_header or first_header
    return header, metadata, {
        "initial_header_columns": len(first_header),
        "effective_header_columns": len(header),
        "header_source": "final" if final_header else "initial",
        "data_rows": data_rows,
        "max_data_row_columns": max_data_width,
    }


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def summarize_optional(
    values: list[float], missing: int, source_indices: set[int], *, timing: bool
) -> dict[str, Any] | None:
    if not values:
        return None
    summary: dict[str, Any] = {
        "samples": len(values),
        "missing_or_invalid_samples": missing,
        "source_column_indices": sorted(source_indices),
    }
    if timing:
        summary.update({
            "median_ms": statistics.median(values),
            "p95_ms": percentile_nearest_rank(values, 0.95),
        })
    else:
        summary.update({
            "min": min(values),
            "max": max(values),
            "median": statistics.median(values),
        })
    return summary


def analyze(
    path: pathlib.Path, start_frame: int, end_frame: int | None,
    min_active_agents: int | None,
) -> dict[str, Any]:
    if not path.is_file():
        raise CsvAnalysisError(f"CSV file does not exist: {path}")
    header, metadata, scan = scan_header(path)
    columns: dict[str, list[int]] = collections.defaultdict(list)
    for index, name in enumerate(header):
        columns[name].append(index)
    if not columns["FrameTime"]:
        raise CsvAnalysisError("FrameTime column is missing")
    if min_active_agents is not None and not columns[CROWD_COLUMNS["active_agents"]]:
        raise CsvAnalysisError(
            "--min-active-agents requires the FFCrowd/ActiveAgents column"
        )

    requested = {
        "frame_time": "FrameTime",
        **TIMING_COLUMNS,
        **CROWD_COLUMNS,
        **MEMORY_COLUMNS,
    }
    samples: dict[str, list[float]] = {key: [] for key in requested if key != "frame_time"}
    missing: dict[str, int] = collections.Counter()
    source_indices: dict[str, set[int]] = collections.defaultdict(set)
    frame_times: list[float] = []
    selected_indices: list[int] = []
    invalid_frame_rows: list[dict[str, int]] = []
    excluded_missing_active = 0
    frame_index = -1
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        for csv_record, row in enumerate(csv.reader(stream), start=1):
            if is_header(row) or is_metadata(row) or not row or not any(cell.strip() for cell in row):
                continue
            frame_index += 1
            if frame_index < start_frame:
                continue
            if end_frame is not None and frame_index >= end_frame:
                break

            if min_active_agents is not None:
                active, _ = number_from_indices(row, columns[CROWD_COLUMNS["active_agents"]])
                if active is None:
                    # This column may have been appended only after Live began.
                    # Earlier rows have no active count and cannot qualify.
                    active_cells = [row[index].strip()
                                    for index in columns[CROWD_COLUMNS["active_agents"]]
                                    if index < len(row)]
                    if any(active_cells):
                        raise CsvAnalysisError(
                            f"FFCrowd/ActiveAgents is nonnumeric at data frame {frame_index} "
                            f"(CSV record {csv_record}); the active-agent filter cannot be trusted"
                        )
                    excluded_missing_active += 1
                    continue
                if active < min_active_agents:
                    continue

            frame_ms, frame_source = number_from_indices(
                row, columns["FrameTime"], positive=True
            )
            if frame_ms is None:
                invalid_frame_rows.append({"frame_index": frame_index, "csv_record": csv_record})
                if len(invalid_frame_rows) >= 10:
                    break
                continue
            frame_times.append(frame_ms)
            selected_indices.append(frame_index)
            source_indices["frame_time"].add(frame_source)
            for key, column_name in requested.items():
                if key == "frame_time":
                    continue
                value, source = number_from_indices(row, columns[column_name])
                if value is None:
                    missing[key] += 1
                else:
                    samples[key].append(value)
                    source_indices[key].add(source)

    if invalid_frame_rows:
        raise CsvAnalysisError(
            "FrameTime must be finite and > 0 in every selected frame; invalid rows: "
            + json.dumps(invalid_frame_rows, separators=(",", ":"))
        )
    if not frame_times:
        raise CsvAnalysisError("No valid frames match the requested selection")

    duration_ms = math.fsum(frame_times)
    unavailable: dict[str, str] = {}

    def optional_group(names: dict[str, str], *, timing: bool) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, column_name in names.items():
            result[key] = summarize_optional(
                samples[key], missing[key], source_indices[key], timing=timing
            )
            if result[key] is None:
                unavailable[key] = (
                    f"column {column_name} absent"
                    if not columns[column_name]
                    else f"column {column_name} has no finite nonnegative samples in selected frames"
                )
        return result

    duplicates = {
        name: indices for name, indices in columns.items() if len(indices) > 1
    }
    return {
        "status": "PASS",
        "method": {
            "time_unit": "milliseconds (UE CSV profiler FrameTime and thread/GPU timings)",
            "mean_fps": "1000 * selected frame count / sum(FrameTime ms)",
            "percentiles": "nearest rank, ceil(p * N), on sorted frame values",
            "hitch_thresholds": "strictly greater than 33.33 ms and 50 ms",
            "frame_indices": "zero-based data rows; --end-frame is exclusive",
        },
        "evidence": {
            "csv_path": str(path.resolve()),
            "csv_bytes": path.stat().st_size,
            "csv_sha256": sha256_file(path),
            **scan,
            "duplicate_columns_by_index": duplicates,
            "selected_source_column_indices": {
                key: sorted(indices) for key, indices in source_indices.items()
            },
            "metadata": metadata,
        },
        "selection": {
            "start_frame": start_frame,
            "end_frame_exclusive": end_frame,
            "min_active_agents": min_active_agents,
            "excluded_frames_without_active_sample": excluded_missing_active,
            "selected_frames": len(frame_times),
            "first_selected_frame": selected_indices[0],
            "last_selected_frame": selected_indices[-1],
            "selected_duration_seconds": duration_ms / 1000.0,
        },
        "frame": {
            "mean_fps": 1000.0 * len(frame_times) / duration_ms,
            "mean_ms": duration_ms / len(frame_times),
            "median_ms": statistics.median(frame_times),
            "p95_ms": percentile_nearest_rank(frame_times, 0.95),
            "p99_ms": percentile_nearest_rank(frame_times, 0.99),
            "hitches_gt_33_33_ms": sum(value > 33.33 for value in frame_times),
            "hitches_gt_50_ms": sum(value > 50.0 for value in frame_times),
        },
        "timings_ms": optional_group(TIMING_COLUMNS, timing=True),
        "crowd": optional_group(CROWD_COLUMNS, timing=False),
        "memory_mb": optional_group(MEMORY_COLUMNS, timing=False),
        "unavailable_fields": unavailable,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=pathlib.Path, help="UE CSV profiler output file")
    parser.add_argument("--output", type=pathlib.Path, help="also save JSON to this path")
    parser.add_argument("--start-frame", type=int, default=0,
                        help="first zero-based CSV data frame to include (default: 0)")
    parser.add_argument("--end-frame", type=int,
                        help="exclusive zero-based CSV data frame boundary")
    parser.add_argument("--min-active-agents", type=int,
                        help="include only frames whose FFCrowd/ActiveAgents reaches this count")
    args = parser.parse_args()
    if args.start_frame < 0 or (args.end_frame is not None and args.end_frame <= args.start_frame):
        parser.error("frame bounds must satisfy 0 <= start-frame < end-frame")
    if args.min_active_agents is not None and args.min_active_agents < 0:
        parser.error("--min-active-agents must be nonnegative")
    if args.output is not None and args.output.resolve() == args.csv_path.resolve():
        parser.error("--output must differ from the input CSV path")

    try:
        result = analyze(args.csv_path, args.start_frame, args.end_frame, args.min_active_agents)
        exit_code = 0
    except (CsvAnalysisError, OSError, UnicodeError, csv.Error) as exc:
        result = {
            "status": "FAIL",
            "evidence": {"csv_path": str(args.csv_path.resolve())},
            "error": str(exc),
        }
        exit_code = 2
    encoded = json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if args.output is not None:
        try:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(encoded, encoding="utf-8")
        except OSError as exc:
            print(f"cannot write JSON output: {exc}", file=sys.stderr)
            return 2
    sys.stdout.write(encoded)
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
