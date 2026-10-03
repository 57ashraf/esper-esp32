# esper-esp32 — v0.1.0 (experimental)

An **unofficial Esper fork**, focused on a classic ESP32-WROOM with 4 MB flash and
**no PSRAM**. This is an **experimental, source-only, trusted-LAN-only** hobby project,
**not a production Pi-hole replacement**.

**Physical-device validation is pending. This v0.1.0 publication build has not
been flashed or physically validated.** Boot, Wi-Fi/DNS/HTTP integration, upgrade
and power-loss recovery, runtime heap soak and Vodafone Power Hub compatibility
have not been verified for this release. Earlier working-device results do not
validate this hardened publication build. Host tests and a placeholder build are
not substitutes for those checks.

Repository name: `esper-esp32`. Maintainer: GitHub
[@57ashraf](https://github.com/57ashraf). Planned version/tag: `v0.1.0`.
The repository and tag have not been created or published by this preparation.

It deliberately uses **ESP-IDF 4.4.7, a legacy EOL SDK**. The v4.4 branch reached
EOL in July 2024 and no longer receives security fixes:
[Espressif's advisory](https://documentation.espressif.com/AR2024-008%20End-of-Life%20Advisory%20for%20ESP-IDF%20v4.4%20Release%20Branch%20EN.html).
Pinning the known toolchain is for compatibility, not a claim of current security.

**Never expose HTTP/80 or DNS/53 directly to the public internet.** Do not enable
port forwarding, an internet-facing DNS proxy, or dashboard access from untrusted
networks. The dashboard has no authentication and optional query logs reveal
browsing metadata to LAN peers. Use a trusted LAN or isolated test network.

## What it does

- IPv4 UDP DNS relay to a configurable external IPv4 resolver (default 8.8.8.8).
- Block A with 0.0.0.0, AAAA with ::, and CNAME/HTTPS questions with NODATA.
- Canonical ASCII case-insensitive, trailing-dot and label-aware suffix/glob matching.
- Collision-safe, flash-resident ESBLv1 blocklist: hashes index candidates; actual
  name bytes decide a hit. Lists are generated on the host, not kept in RAM.
- Read-only embedded LAN dashboard, /status.json and an optional 100-entry log.
- Raw allowed DNS record forwarding with bounded validation, EDNS and compression
  handling, source/question correlation and unique pending upstream IDs.

No web configuration, blocklist edits, restart, OTA or update checks, arbitrary
file serving, caching, automatic list updates or CNAME-response filtering.
No client-facing TCP/53; see [limitations](docs/LIMITATIONS.md).

## Build, install and verify

1. Read [bootstrap and clean build](docs/BUILD.md).
2. Obtain HaGeZi Multi LIGHT separately and [generate a local ESBL](software/tools/blocklist/README.md).
3. Follow [first-install versus upgrade guidance](docs/INSTALL.md). Never overwrite
   an existing filesystem image or erase NVS as an upgrade shortcut.
4. Review [validation](docs/VALIDATION.md), [security](SECURITY.md) and the
   [release checklist](docs/RELEASE_CHECKLIST.md).

There is no browser flasher or downloadable firmware in this release.
Credentials must be entered locally, never committed. They remain readable in
local sdkconfig, compiled firmware and LittleFS/NVS; this is not an encrypted-secret
storage design. CI uses placeholders only and publishes no firmware artifacts.

## Attribution and validation status

Esper's MIT attribution is preserved: Copyright (c) 2021 Zach Morris.
See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md) for inherited versus fork-authored
functionality and third-party licenses. No hardware designs or HaGeZi data are
distributed here.

This v0.1.0 publication build has **not been flashed or hardware-validated**.
ESP32 heap-soak, actual DNS/HTTP integration, upgrade/power-loss recovery and
Vodafone Power Hub regression tests remain **pending**. Host tests and a clean
placeholder build are useful evidence, not substitutes for those tests.
