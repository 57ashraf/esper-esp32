#!/usr/bin/env python3
"""Create a NEW, PRIVATE first-install LittleFS input tree; never edits an existing tree."""
from pathlib import Path
import argparse
import json
import re
import shutil

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--sdkconfig",type=Path,required=True)
    parser.add_argument("--blocklist",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists(): raise SystemExit("Refusing an existing output directory; preserve it as a backup.")
    # Validate ESBL before preparing any credentials. No network downloads or flashing.
    import sys
    root=Path(__file__).resolve().parents[1]
    sys.path.insert(0,str(root/"software/tools/blocklist"))
    from blocklist_format import read_metadata
    image=args.blocklist.read_bytes()
    read_metadata(image)
    if len(image)>int(1769472*0.90): raise SystemExit("Blocklist exceeds conservative filesystem budget.")
    config=json.loads((root/"examples/settings.example.json").read_text(encoding="utf-8"))
    source=args.sdkconfig.read_text(encoding="utf-8")
    for key, target in (("SSID","ssid"),("PASSWORD","password")):
        match=re.search(r'^CONFIG_WIFI_'+key+r'=(".*")$',source,re.M)
        if not match: raise SystemExit("Missing Wi-Fi configuration; use local menuconfig.")
        config[target]=json.loads(match.group(1)) # proper escaped-quote handling
    if not config["ssid"] or len(config["ssid"].encode("utf-8"))>32 or len(config["password"].encode("utf-8"))>64:
        raise SystemExit("Invalid Wi-Fi configuration.")
    directory=args.output/"ota_0"
    directory.mkdir(parents=True)
    (directory/"settings.json").write_text(json.dumps(config,ensure_ascii=False),encoding="utf-8")
    (directory/"blacklist.txt").write_text("",encoding="utf-8")
    shutil.copyfile(args.blocklist,directory/"blocklist.bin")
    print("Private first-install input created. It contains credentials: do NOT publish it.")
if __name__=="__main__": main()
