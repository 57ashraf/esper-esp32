#!/usr/bin/env python3
"""Benchmark indexed lookup and report size/RAM-oriented measurements."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import time
import tracemalloc

from blocklist_format import IndexedBlocklist, read_metadata


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--queries", type=int, default=10000)
    args = parser.parse_args()

    data = args.image.read_bytes()
    metadata = read_metadata(data)
    tracemalloc.start()
    indexed = IndexedBlocklist(data)
    _, peak_construct = tracemalloc.get_traced_memory()

    # Read the first records through the public image layout to make positive
    # probes deterministic without adding a second domain list to the tool.
    positives = []
    for index in range(min(args.queries, metadata.entry_count)):
        _digest, string_off, length, _flags = indexed._record(index)
        start = metadata.string_offset + string_off
        positives.append(data[start : start + length].decode("ascii"))
    if not positives:
        positives = ["example.invalid"]
    positives = (positives * ((args.queries + len(positives) - 1) // len(positives)))[: args.queries]
    known_safe = [
        "example.com",
        "example.org",
        "example.net",
        "ietf.org",
        "wikipedia.org",
        "microsoft.com",
        "openai.com",
        "cloudflare.com",
        "python.org",
        "github.com",
    ]
    negatives = [
        known_safe[i % len(known_safe)] if i < len(known_safe) * 10
        else f"unlisted-{i}.invalid"
        for i in range(args.queries)
    ]

    def timed(values):
        results = []
        start = time.perf_counter_ns()
        for value in values:
            results.append(indexed.lookup(value))
        elapsed = time.perf_counter_ns() - start
        return elapsed, results

    # Warm up interpreter and branch paths before measuring.
    timed(positives[: min(100, len(positives))])
    hit_ns, hits = timed(positives)
    miss_ns, misses = timed(negatives)
    _current, peak_total = tracemalloc.get_traced_memory()
    tracemalloc.stop()

    report = {
        "file_size": len(data),
        "entry_count": metadata.entry_count,
        "wildcard_count": metadata.wildcard_count,
        "positive_queries": len(positives),
        "negative_queries": len(negatives),
        "positive_hits": sum(hits),
        "negative_false_positives": sum(misses),
        "known_safe_false_positives": sum(indexed.lookup(value) for value in known_safe),
        "positive_total_ms": hit_ns / 1_000_000,
        "negative_total_ms": miss_ns / 1_000_000,
        "positive_mean_us": hit_ns / len(positives) / 1_000,
        "negative_mean_us": miss_ns / len(negatives) / 1_000,
        "python_peak_traced_bytes": peak_total,
        "python_peak_construct_bytes": peak_construct,
        # Firmware opens the file and uses bounded buffers rather than the
        # Python object.  This is the deliberately bounded scratch estimate.
        # 40-byte metadata + two 16-byte records/raw buffers + two 254-byte
        # name buffers + two 128-entry LabelSpan arrays (8 bytes per span on
        # the 32-bit ESP32).  This excludes transient std::string heap space
        # and the already allocated DNS task stack.
        "firmware_bounded_scratch_bytes": 40 + 16 + 16 + 254 + 254 + 2 * 128 * 8,
        "lookup_false_positive_rate": sum(misses) / len(misses) if misses else 0,
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
