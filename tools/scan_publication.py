#!/usr/bin/env python3
"""Conservative exact-tree source/secret checks. Never prints matching secret values."""
from pathlib import Path
import argparse
import ast
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
BAD_DIRS = {"hardware", "backup", "backups", "rollback", "deploy", "deployment", "snapshot",
            "snapshots", "private", "data", "generated", "build", "cmakefiles", "__pycache__", "node_modules"}
BAD_SUFFIX = {".bin", ".elf", ".map", ".o", ".a", ".exe", ".dll", ".zip", ".7z", ".tar", ".gz",
              ".pyc", ".bak", ".backup", ".log", ".pem", ".key", ".crt", ".p12", ".pfx", ".kicad_pcb", ".sch", ".brd"}
PATTERNS = {
    "private key/certificate": rb"-----BEGIN (?:[A-Z ]*PRIVATE KEY|CERTIFICATE)-----",
    "GitHub token": rb"\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{30,})\b",
    "AWS key": rb"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b",
    "JWT": rb"\beyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\b",
    "local Windows path": rb"(?i)(?:[A-Z]:[\\/](?:Users|Documents|Temp)[\\/]|file:\x2f\x2f)",
    "local Unix path": rb"(?i)/(?:home|Users)/[a-z0-9_.-]+/",
    "secret assignment": rb'''(?i)\b(?:api_key|access_token|secret_key|authorization)\s*[:=]\s*["'](?!["'])[^"'\r\n]{8,}''',
}
def scan(root, known=None):
    errors = []
    count = 0
    for p in sorted(root.rglob("*")):
        relative = p.relative_to(root)
        if ".git" in relative.parts:
            continue # Source snapshot only; history review is a separate release gate.
        if p.is_symlink():
            errors.append((str(relative), "symlink")); continue
        if not p.is_file():
            continue
        count += 1
        parts = [part.lower() for part in relative.parts]
        if any(part in BAD_DIRS or part.startswith("build-") for part in parts):
            errors.append((str(relative), "unwanted directory"))
        config_name = p.name.lower()
        private_config = (config_name.startswith("sdkconfig.") and
                          config_name != "sdkconfig.defaults" and
                          relative.as_posix() != "examples/sdkconfig.example")
        if (p.suffix.lower() in BAD_SUFFIX or private_config or config_name.startswith(".env") or
                config_name in {"sdkconfig", "settings.json", "blacklist.txt", "blocklist.bin"}):
            errors.append((str(relative), "unwanted artifact/config"))
        content = p.read_bytes()
        if b"\x00" in content:
            errors.append((str(relative), "non-source/binary content")); continue
        try: text = content.decode("utf-8")
        except UnicodeDecodeError: errors.append((str(relative),"non-UTF8 source")); continue
        for name, pattern in PATTERNS.items():
            if re.search(pattern, content):
                errors.append((str(relative), name))
        for kind, secret in (known or []):
            if secret and secret in content:
                errors.append((str(relative), "known private "+kind))
        for match in re.finditer(r'(?:CONFIG_WIFI_(?:SSID|PASSWORD)\s*=\s*"|"(?:ssid|password)"\s*:\s*")([^"]*)"', text):
            if match.group(1) not in {"", "fixture-ssid", "fixture-password", "CI_PLACEHOLDER_SSID", "CI_PLACEHOLDER_PASSWORD"}:
                errors.append((str(relative), "non-placeholder Wi-Fi value"))
        if p.suffix == ".py":
            try: ast.parse(text, filename=str(relative))
            except SyntaxError: errors.append((str(relative), "Python syntax"))
    # Security regression gates apply to the shipped source, not explanatory docs.
    fw = root/"software/firmware"
    js = (fw/"components/flash/files/app_scripts.js").read_text(encoding="utf-8")
    if re.search(r"innerHTML|outerHTML|insertAdjacentHTML|document\.write", js):
        errors.append(("dashboard","unsafe DOM sink"))
    if "textContent" not in js: errors.append(("dashboard","missing textContent"))
    http = "\n".join(p.read_text(encoding="utf-8") for p in (fw/"components/http").rglob("*.cpp"))
    if re.search(r"HTTP_POST|HTTP_PUT|HTTP_DELETE|fs::open|restart|update_srv|setting::PASSWORD|setting::SSID",http):
        errors.append(("HTTP","mutation/filesystem/secret access"))
    filesystem = (fw/"components/flash/filesystem.cpp").read_text(encoding="utf-8")
    if "format_if_mount_failed = false" not in filesystem:
        errors.append(("storage","automatic format protection missing"))
    wifi = (fw/"components/netif/wifi.cpp").read_text(encoding="utf-8")
    if re.search(r"ESP_LOG[^\n]*(?:ssid|pass|SSID|PASS)",wifi):
        errors.append(("Wi-Fi","credential logging"))
    return count, errors

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root",type=Path,default=ROOT)
    parser.add_argument("--known-sdkconfig",type=Path,help="Optional private config to compare; values are never printed")
    args = parser.parse_args()
    known = []
    if args.known_sdkconfig:
        content = args.known_sdkconfig.read_text(encoding="utf-8")
        for kind in ("SSID","PASSWORD"):
            match = re.search(r'^CONFIG_WIFI_'+kind+r'=(".*")$',content,re.M)
            if match:
                try: value = json.loads(match.group(1))
                except json.JSONDecodeError: raise SystemExit("Cannot decode private Wi-Fi value; no value printed.")
                known.append((kind,value.encode("utf-8")))
    count, errors = scan(args.root.resolve(),known)
    for path, reason in errors: print(f"FAIL: {path}: {reason}")
    print(f"Scanned {count} exact-tree files; {len(errors)} findings (no secret values printed).")
    return int(bool(errors))
if __name__ == "__main__":
    sys.exit(main())
