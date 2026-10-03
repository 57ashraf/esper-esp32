#!/usr/bin/env python3
"""Generate collision-safe ESBLv1 locally; never downloads or flashes data."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import tempfile
from blocklist_format import build_binary, parse_rules

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--littlefs-bytes", type=int, default=1728*1024)
    parser.add_argument("--max-rules", type=int, default=50000)
    args = parser.parse_args()
    if args.littlefs_bytes <= 0 or args.max_rules <= 0:
        parser.error("Capacity and max-rules must be positive")
    source_bytes = args.source.read_bytes()
    rules = parse_rules(source_bytes.decode("utf-8-sig").splitlines())
    if not rules or len(rules) > args.max_rules:
        parser.error("Empty list or rule count exceeds conservative limit; review the source")
    image, stats = build_binary(rules)
    if len(image) > int(args.littlefs_bytes * 0.90):
        parser.error("ESBL exceeds 90% payload budget; do not truncate the list silently")
    if args.output.exists():
        parser.error("Output exists; keep it as rollback data and generate to a new filename")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=args.output.parent, prefix=".esbl-", delete=False) as f:
            temporary = Path(f.name)
            f.write(image); f.flush(); os.fsync(f.fileno())
        # Atomic create-without-replace: do not consume an existing rollback file.
        # A host filesystem without hard-link support fails safely here.
        os.link(temporary, args.output)
    finally:
        if temporary is not None: temporary.unlink(missing_ok=True)
    report = {
        "source_bytes": len(source_bytes), "parsed_rules": len(rules), **stats,
        "littlefs_capacity": args.littlefs_bytes,
        "littlefs_payload_percent": 100.0 * len(image) / args.littlefs_bytes,
        "littlefs_payload_remaining_bytes": args.littlefs_bytes-len(image),
        "note": "Raw payload budget only; check the complete LittleFS image separately",
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0
if __name__ == "__main__":
    raise SystemExit(main())
