# esper-esp32 — v0.1.1 (experimental)

[![Source validation](https://github.com/57ashraf/esper-esp32/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/57ashraf/esper-esp32/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

An **unofficial Esper fork**: a memory-bounded DNS ad blocker for a classic
ESP32-WROOM with **4 MB flash and no PSRAM**. **Experimental, source-only,
trusted-LAN-only — not a production Pi-hole replacement.**

**Physical-device validation is pending. This v0.1.1 build has not been flashed or
physically validated.** Boot, Wi-Fi/DNS/HTTP integration, heap soak, upgrade and
power-loss recovery, and Vodafone Power Hub compatibility remain unverified.
Earlier working-device results and the v0.1.0 release do not validate this build.
Host tests, loopback sockets, sanitizers and a clean target build are not substitutes.

**Never expose HTTP/80 or DNS/53 directly to the public internet.** The dashboard
has no authentication or encryption. Optional query logs reveal browsing metadata
to LAN peers. Use a trusted LAN or isolated test network, with no public forwarding.

It deliberately pins **ESP-IDF 4.4.7, a legacy EOL SDK**. The v4.4 branch reached
EOL in July 2024; pinning it is for compatibility, not current security.
[Espressif's advisory](https://documentation.espressif.com/AR2024-008%20End-of-Life%20Advisory%20for%20ESP-IDF%20v4.4%20Release%20Branch%20EN.html).

Repository: [57ashraf/esper-esp32](https://github.com/57ashraf/esper-esp32).
Maintainer: GitHub [@57ashraf](https://github.com/57ashraf).
[Releases](https://github.com/57ashraf/esper-esp32/releases) are experimental source
snapshots; there are **no downloadable firmware images or browser flasher**.

## What it does

- IPv4 UDP DNS relay to an external IPv4 resolver (default 8.8.8.8), using a
  separate ephemeral upstream socket.
- A blocking with 0.0.0.0, AAAA with ::, CNAME/HTTPS questions with NODATA.
- ASCII case-insensitive, trailing-dot and label-aware suffix/glob matching.
- Collision-safe ESBLv1 index in LittleFS: hashes find candidates; actual name
  bytes decide hits. The domain list is not loaded into RAM.
- Read-only embedded dashboard, safe /status.json and optional 100-entry query log
  (disabled by default).
- Bounded DNS compression/EDNS validation, exact source/question correlation,
  unique pending transaction IDs and size-safe forwarding.
- Upstream TCP retry on a truncated UDP answer, with a single three-second deadline.
  **This is not a client-facing TCP/53 service.**

No web configuration, mutation routes, OTA, arbitrary file serving, DNS caching,
automatic blocklist updates, encrypted upstream or CNAME-response filtering.
See [limitations](docs/LIMITATIONS.md) before relying on it.

## New in v0.1.1

- Prompt SERVFAIL on upstream send failure or a full transaction table; idle timeout
  cleanup releases slots and answers expired requests.
- Local blocked/error replies acknowledge EDNS0 without echoing client options or
  claiming DNSSEC authentication.
- Domain-free, atomic DNS counters help distinguish blocked queries, malformed
  packets, overload, timeouts and failed TCP retries.
- Dashboard Host/origin checks reject browser DNS rebinding and foreign-origin
  reads. They do **not** authenticate LAN users or prevent a hostile LAN peer.
- Actual forwarding-state fault tests, real loopback TCP/UDP tests, portable
  Windows/POSIX tests and Linux ASan/UBSan CI.

ESBLv1, rule semantics, partition layout and private settings remain unchanged.
[Changelog](CHANGELOG.md) · [design](docs/DNS_RELIABILITY.md) · [validation](docs/VALIDATION.md)

## Build and try it safely

1. Read [bootstrap and clean build](docs/BUILD.md); obtain the pinned SDK separately.
2. Obtain HaGeZi Multi LIGHT separately and [generate an ESBL locally](software/tools/blocklist/README.md).
   **No HaGeZi data is bundled or relicensed as MIT.**
3. Enter credentials locally using menuconfig; keep private settings, build outputs
   and generated data out of Git.
4. Read [first install versus upgrade](docs/INSTALL.md). Preserve a private rollback
   build and filesystem/NVS backups. Never use a full flash/filesystem replacement
   as an upgrade shortcut.
5. Test direct DNS on one device before any manually reviewed router change.
   [Troubleshooting](docs/TROUBLESHOOTING.md) includes read-only probes and loop warnings.

Wi-Fi credentials remain readable in local sdkconfig, compiled firmware and
LittleFS/NVS; this is not encrypted-secret storage. CI uses placeholders only and
publishes no firmware artifacts. Source publication does not authorize or perform
hardware flashing, device validation or changes to household network settings.

## Tests, contribution and attribution

[Tests](tests/README.md) link actual firmware logic and separately exercise the
upstream TCP transport on loopback. [CI](.github/workflows/ci.yml) adds a clean
SDK-pinned placeholder build and Linux address/undefined-behavior sanitizers.
A green badge is not hardware validation or a security guarantee.

Esper's MIT attribution is preserved: Copyright (c) 2021 Zach Morris.
See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md) for inherited versus fork-authored
work and vendored MIT/BSD notices. No hardware designs or external blocklists are
distributed. [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md)
