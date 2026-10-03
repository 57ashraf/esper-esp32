# Attribution and dependency provenance

This is an **unofficial software-only fork of [Esper](https://github.com/zachmorr/esper)**,
not an official Esper release and not affiliated with Espressif, Vodafone, Pi-hole
or HaGeZi. The upstream MIT notice, Copyright (c) 2021 Zach Morris, is retained in
[LICENSE](LICENSE). Fork-authored firmware, tools, tests and docs are offered under
that MIT license; no hardware designs are included.

Fork maintainer and contributor: GitHub [@57ashraf](https://github.com/57ashraf).
Copyright (c) 2026 57ashraf, for fork-authored modifications. This additional
credit does not replace Esper's or any vendored dependency's original notices.
Public repository name: `esper-esp32`; planned release version/tag: `v0.1.0`.

Inherited: ESP-IDF startup/network skeleton, settings keys, UDP DNS filtering and
relay, embedded HTTP interface structure, LittleFS integration and partition layout.
Earlier fork work retained: case-insensitive label-aware suffix/glob semantics,
ESBLv1 FNV-1a index with exact-name collision verification, external upstream via an
ephemeral UDP socket, raw allowed-packet forwarding and upstream TCP retry on TC.
v0.1.0 changes: conservative protocol/resource bounds, unique outstanding IDs,
read-only diagnostics, optional synchronized query logging, safe settings/storage,
explicit bootstrap/install/upgrade documentation and host regression tests.

Vendored dependencies (source and original licenses only):

| Dependency | Pin / origin | License |
| --- | --- | --- |
| esp_littlefs wrapper | v1.4.1, commit 4a5121096bea32ac908735d971cffd34e5fe280f, Brian Pugh | MIT, components/littlefs/LICENSE |
| littlefs core | v2.5, commit 40dba4a556e0d81dfbe64301a6aa4e18ceca896c | BSD-3-Clause, components/littlefs/src/littlefs/LICENSE.md |
| cJSON (host tests) | 1.7.17, source matching the local ESP-IDF JSON dependency | MIT, tests/vendor/cJSON/LICENSE |

The vendored wrapper has local descriptor bounds/null checks, close-error resource
cleanup and locked write-sync hardening. It is not an unmodified upstream snapshot.

ESP-IDF is **not bundled**. Obtain exactly v4.4.7 from Espressif; its dependencies
carry their own licenses. The native test compiler is downloaded separately,
checksum-pinned, and is not publication material.

HaGeZi data is **not bundled or relicensed under MIT**. Obtain it separately from
[the upstream project](https://github.com/hagezi/dns-blocklists), retain provenance,
and read its [GPL-3.0 license](https://github.com/hagezi/dns-blocklists/blob/main/LICENSE)
and disclaimer. A locally generated ESBL contains that data; do not assume the
firmware MIT license permits arbitrary redistribution of the generated dataset.
No third-party logos, hardware CAD, compiled firmware, credentials or private data
are included.
