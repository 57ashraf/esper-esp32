#!/usr/bin/env python3
"""Report real binary/partition sizes without printing local build paths."""
from pathlib import Path
import argparse
import csv
import json

def number(value):
    value=value.strip()
    if value[-1:].upper()=="K": return int(value[:-1],0)*1024
    if value[-1:].upper()=="M": return int(value[:-1],0)*1024*1024
    return int(value,0)
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("build",type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    rows=csv.reader(line for line in (root/"software/firmware/partitions_table.csv").read_text().splitlines() if not line.startswith("#"))
    slots={row[0].strip():number(row[4]) for row in rows if len(row)>4 and row[1].strip()=="app"}
    binary=args.build/"EsperExperimental.bin"
    size=binary.stat().st_size
    report={"application_slots_bytes":slots,"firmware_binary_bytes":size,
            "slot_headroom_bytes":{slot:capacity-size for slot,capacity in slots.items()},
            "minimum_goal_bytes":64*1024}
    print(json.dumps(report,indent=2,sort_keys=True))
    if any(capacity-size<64*1024 for capacity in slots.values()): raise SystemExit("Application slot headroom below 64 KiB")
if __name__=="__main__": main()
