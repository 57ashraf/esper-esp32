# v0.1.0 validation and final publication review

Final review: **2026-10-03**; initial validation: 2026-10-02.
Repository: `esper-esp32`; planned version/tag: `v0.1.0`; maintainer: GitHub `@57ashraf`.
Scope: separate source-only publication copy, classic
ESP32-WROOM target, 4 MB flash/no PSRAM. No device was probed/flashed, no Wi-Fi,
PC/router settings changed, and no GitHub repository/commit/push/deployment occurred.

## Results

| Gate | Actual local result |
| --- | --- |
| ESP-IDF | Exact v4.4.7 tag, commit 38eeba213aa695aabfd6d89aa9f5078dbe5a94c3 |
| Target compiler | Xtensa GCC 8.4.0, esp-2021r2-patch5 |
| Clean build | PASS, new build directory and config; explicit CI_PLACEHOLDER credentials |
| Default query log | Disabled in the measured firmware |
| Native normal/logging-on C++ suite | PASS, 6,436 checks |
| Native forced-hash-collision C++ suite | PASS, 6,437 checks |
| Native logging-disabled C++ suite | PASS, 1,435 checks |
| Malformed DNS smoke | 20,000 deterministic inputs per native suite (60,000 executions; same seed), no unexpected exception |
| Python ESBL tests | PASS, 9 tests |
| Python release/tooling tests | PASS, 9 tests, including release identity, exact documented tree and private-config/log exclusions |
| Actual LittleFS core model | PASS, 47 reported checks; sync-fault, rename and remount tests |
| Exact-tree publication/secret scan | PASS; **105 files**, no findings, including private Wi-Fi value comparison |
| Production hash-override guard | PASS; host-test hash override rejected outside host-test builds |
| Original project | Git status matches starting state; final fingerprints of 165 tracked/config/build files unchanged |
| Remote GitHub CI | NOT RUN; workflow definition only |
| On-device / Power Hub / heap soak | PENDING; no flashing authorized in this release-preparation scope |

Native tests compile the **actual firmware implementation**, not a Python port:
DNS parse/serialize, transaction matcher/ID selection, ESBL verifier/lookup,
settings, file wrapper, query log, dashboard serializers/routes, HTTP registration
and GET handlers. Platform transports/locks/time and embedded asset bytes are
shimmed. Actual vendored LittleFS C sources run separately on a host RAM-flash
model. The ESP-IDF network, NVS, Wi-Fi and real HTTP transport have not been tested
on hardware for v0.1.0.

Coverage includes short/malformed packets, name/record bounds, self/forward
compression pointers, high 14-bit pointer offsets, 255-octet name boundary,
compressed CNAME/owners, EDNS versions/options/duplicates, record types,
byte-preserving allowed A/AAAA/HTTPS responses, zero-address/NODATA local replies,
transaction collisions, upstream source/question/class mismatch, ID restoration,
512/EDNS truncation, index corruption/reserved bytes/canonical/hash validation,
forced collisions, rule semantics, write-sync/rename failures, move-only files,
concurrent settings/log reads, log-off mode, safe JSON, secret/mutation/file routes,
generator reproducibility/capacity/overwrite protection and first-install tooling.

## Exact flash and static memory measurements

Default logging-off placeholder build:

| Measurement | Bytes |
| --- | ---: |
| ota_0 application partition | 1,179,648 |
| ota_1 application partition | 1,179,648 |
| Actual EsperExperimental.bin file | **845,792** |
| Actual binary headroom per slot | **333,856** |
| Desired minimum headroom | 65,536 |
| LittleFS partition | 1,769,472 |
| Static DRAM total | **29,100** |
| .data | 13,332 |
| .bss | 15,768 |
| Static IRAM total | **91,942** |
| IRAM .text | 90,915 |
| IRAM vectors | 1,027 |
| Mapped flash .text | 603,911 |
| Mapped flash .rodata | 137,739 |
| idf_size aggregate image estimate | 847,180 |

Headroom is calculated from the **actual binary file**, not the map estimate.
The SDK map tool reports 151,636 remaining DRAM and 39,130 remaining IRAM;
these linker-region numbers are **not measured runtime free heap**.
Binaries, maps, generated config, logs and toolchain copies are private validation
artifacts outside the publication tree. The existing working build remains intact.
The final from-empty placeholder build includes the v0.1.0 dashboard and startup
labels; its binary is 144 bytes larger than the initial preparation build.

Target ABI size probe (compiled only, never executed/flashed):
DNS=112, Client=64, Log_Entry=40, cJSON=40, std::string=24,
byte-vector=12, Header=12, Question=16 bytes.

## Known bounded dynamic allocations

These are source/ABI bounds, **not an on-device peak-heap measurement**. Allocator,
stdio, FreeRTOS, lwIP, Wi-Fi and SDK overhead remain additional.

| Allocation | Bound / behavior |
| --- | --- |
| DNS listener + worker stacks | 8,000 + 15,000 = 23,000 bytes |
| HTTP worker stack | SDK default 4,096 bytes; at most 3 open HTTP sockets |
| Receive scratch | 4,097 bytes; detects datagrams exceeding 4,096 |
| Received DNS objects | Queue 8, plus worker/receiver objects; 112-byte object + raw packet at most 4,096 each |
| Validated question name | At most 255 wire octets, plus bounded vector capacity |
| Printable parsed domain | At most about 1,012 characters for escaped binary labels; ordinary hostnames at most 253 |
| Pending clients | Maximum 16; table objects 16 x 64 = 1,024 bytes |
| Stored original query per client | At most 2,048 bytes; payload total at most 32,768 |
| Pending question key | At most 259 bytes plus string capacity, retains wire label boundaries/type/class |
| TCP retry | At most one worker retry; response at most 4,096 bytes, plus parsed/copy scratch; absolute I/O deadline 3 seconds |
| Settings | Input at most 4,096 + NUL; maximum 16 flat fields, 32-byte keys, bounded values; old/candidate cJSON trees and printed temp string coexist while committing |
| ESBL | Retained metadata 36 bytes; no in-RAM list/table; 254-byte name buffers and bounded label scratch |
| Generic glob scratch | Two 128-entry LabelSpan arrays; 2,048 bytes on the 32-bit target, on the existing worker stack |
| LittleFS | Read/program caches 512 bytes each, lookahead 128 bytes, plus 512-byte cache per open file and VFS/stdio overhead |
| Query logging (off by default) | When enabled: 100 x 40-byte entries plus at most 100 x 254-byte domain storage, about 29,400 payload bytes |
| Query-log HTTP snapshot | A further bounded copy of up to 100 entries, about 29,400 payload bytes, plus per-entry JSON scratch |
| C++ emergency exception pool | Configured 1,024 bytes, SDK-managed |

Individual limits are not all simultaneous, and string/vector growth capacity can
exceed logical lengths. No fabricated total peak/fragmentation figure is claimed.
Logging-on worst-case load, queue pressure and largest free heap block require
the pending ESP32 soak. Keep query logging disabled for conservative operation.

## C++ lookup and filesystem benchmark

Synthetic, non-distributed fixture: 35,000 ordinary suffix rules named under
blocked.test. It is **not HaGeZi data** and does not measure real-world list quality.

- ESBL size: **1,225,040 bytes**.
- Reversing input order produced exactly identical ESBL bytes.
- Actual filesystem-backed C++ lookup: **10,000/10,000 hits**.
- Negative probes: **0/10,000 false positives**.
- The final-review host run: full validation **137.976 ms**;
  hit mean **50.5615 microseconds**, miss mean **91.1 microseconds**.
- Results vary with host/storage load; other runs under concurrent compilation
  were slower (including a 1,240.09 microsecond mean miss result). These are not
  ESP32 flash lookup timings and not an on-device RAM benchmark.
- Actual littlefs v2.5 core, disk format 2.0, 432 x 4,096-byte blocks:
  **304 allocated blocks = 1,245,184 bytes**, **128 free blocks = 524,288 bytes**
  after storing the synthetic ESBL and settings/temp-write test fixtures.
- RAM-flash model verified failed temp sync leaves the destination intact and
  committed rename survives unmount/remount. This is **not a physical power-cut
  or ESP32 VFS integration test**.

## Scan and review

The exact export was scanned recursively, including hidden CI/ignore files.
Checks cover binary/generated/config/backup/hardware filenames, NUL/binary content,
private/local paths, key/certificate markers, common token signatures, non-placeholder
Wi-Fi values, unsafe DOM sinks, secret/filesystem/mutation HTTP references and
automatic-format protection. Private Wi-Fi SSID/password were compared in memory;
their values were never printed or copied into release documentation.

The scan is heuristic, not a proof about arbitrary secrets or future Git history.
The final scan output is the authoritative file count; publication-tree documentation
lists all intended files. Future edits must rerun tests/scan and be reviewed.
No original Git history is copied and no repository is initialized here.

## Final public-content review — 2026-10-03

- The authenticated GitHub connector confirmed the user's login as `57ashraf`.
  Attribution uses that GitHub identity, not a legal name or personal email.
- README now leads with the lack of physical-device validation and explicitly
  distinguishes earlier working-device results from this unflashed release.
- Esper's original MIT LICENSE is byte-for-byte identical to the original project.
  Existing upstream and vendored attribution notices remain intact; fork credit
  was added separately in NOTICE.md.
- Corrected a generated documentation defect that dropped leading characters
  from tree entries. A new test compares the complete documented tree against
  the exact export, including hidden files.
- Tightened ignore/export rules and scanner checks for additional sdkconfig files,
  settings, environment files, logs, private key containers and archive artifacts.
- Reran all native suites, LittleFS model, 18 Python tests, a clean placeholder
  build and the known-private-Wi-Fi comparison. The exact 105-file export has zero
  scan findings, no Git metadata and no symlinks/reparse points.
- Rechecked original fingerprints: 165 tracked/config/build files, zero changes.
  Original Git status still matches the previously recorded working state.

No remaining private-content finding was identified. This is a reviewed source
snapshot, not a guarantee about arbitrary secrets, future edits or future Git history.
Do not publish the surrounding workspace/private validation directories.

## Warnings and pending acceptance

The SDK build emitted legacy Python Click and certificate-bundle deprecation
warnings; the placeholder build still succeeded. Initial local tool discovery/
header-access issues were resolved with a separate private compiler copy, without
changing the original SDK/project or installing globally.
The initial final-review build emitted Git ownership warnings for the private
dependency checkout under a different execution account. These were resolved with
process-scoped trust for the verified SDK checkout and metadata-only reconfiguration;
the measured build records SDK version v4.4.7, and the exact release commit was
checked separately. No global Git settings changed.

Pending after a separately authorized flash: ESP32-WROOM boot, retained NVS/LittleFS,
Wi-Fi/LAN IP, actual UDP/HTTP integration, external upstream and TCP retry behavior,
direct blocked/allowed/EDNS probes, Power Hub manual upstream path, heap/fragmentation
soak with log off/on, reconnect/load/queue stress and physical power-loss/upgrade
recovery. Client TCP/53 remains deliberately unsupported.

The copy is suitable for **reviewed experimental source publication**, not a
hardware-validated/stable release. The user selected `esper-esp32`, `v0.1.0` and
GitHub-only attribution to `@57ashraf`; no repository or tag has been created.
Physical validation, remote CI execution, and any later flash/publish/deploy
decision remain pending and are not authorized by this preparation.
