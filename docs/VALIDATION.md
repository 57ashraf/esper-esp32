# v0.1.1 validation report — 2026-10-04

Experimental, source-only, trusted-LAN-only ESP32-WROOM 4 MB/no PSRAM target.
**No physical device was probed, flashed or validated.** Wi-Fi/lwIP/HTTP integration,
Power Hub compatibility, runtime heap soak, upgrade and electrical power-cut
recovery remain pending. No PC/router/network settings or original project changed.

## Evidence and its limits

| Gate | Result / scope |
| --- | --- |
| SDK | Exact ESP-IDF v4.4.7, commit 38eeba213aa695aabfd6d89aa9f5078dbe5a94c3; legacy EOL |
| Target compiler | Xtensa GCC 8.4.0 / esp-2021r2-patch5 |
| Placeholder target build | PASS; new empty build/config, CI_PLACEHOLDER credentials, logging off |
| Windows normal C++ | PASS, 6,542 checks |
| Windows forced-hash-collision C++ | PASS, 6,543 checks |
| Windows logging-disabled C++ | PASS, 1,541 checks |
| Malformed DNS smoke | 20,000 deterministic bounded inputs per mode; 60,000 executions of the same seed |
| Real loopback transport | PASS, 51 checks; actual upstream TCP and UDP send helpers |
| Actual LittleFS core model | PASS, 47 checks; sync fault, atomic rename, remount and filesystem fit |
| Python blocklist/release/tooling | PASS, 18 tests |
| Exact export/known-private-value scan | PASS; 115 files, zero findings; private values compared in memory, never printed |
| Original project integrity | 165 tracked/config/build fingerprints unchanged; previous publication copy retained |
| Windows/POSIX/SDK cloud CI | Required release gate; exact run/commit results are linked in release notes |
| Physical board / Power Hub / heap soak | PENDING, not authorized by source publication |

The v0.1.0 public [push](https://github.com/57ashraf/esper-esp32/actions/runs/37113138153)
and [tag](https://github.com/57ashraf/esper-esp32/actions/runs/37113405870) CI passed.
Those older runs do not validate v0.1.1. The new workflow adds Linux ASan/UBSan,
verifies the SDK checkout SHA and retains no-artifact Windows/placeholder-build jobs.
Check the exact release commit, not merely a green default-branch badge.

Native tests link the actual parser/serializer, DnsForwarder state machine,
transaction source/question/ID logic, collision-safe ESBL, settings, file wrapper,
query logging, dashboard JSON/routes and HTTP registration/GET handlers. Transport
faults are injected through the production forwarder's delivery seam. A separate
real-loopback executable links the actual tcp_transport.cpp and UDP send helper.
Only Windows/POSIX OS transport is exercised there; ESP-IDF/lwIP behavior is pending.

The FreeRTOS receive loop, task creation/startup barrier/queue, Wi-Fi/NVS and real
ESP-IDF HTTP transport are target-compiled and reviewed, not host-integrated or
device-tested. ASan/UBSan are not thread sanitizers or proofs of security.

Coverage includes compression/name/RR/EDNS bounds, blocked A/AAAA/CNAME/HTTPS and
byte-preserving allowed A/AAAA/HTTPS, malformed inputs, source/type/ID collisions,
out-of-order delivery, duplicate/late unmatched replies, full-table rejection,
idle expiration, send exceptions and failed client delivery, validated/invalid TCP
retry, partial/dribbling frames, total deadlines and EOF/length/connection errors.
Dashboard tests cover safe JSON, no secret/mutation/file routes, Host/origin/site
guards, missing/oversized headers and legitimate homepage navigation.
ESBL corruption, exact-byte collisions, generator reproducibility/capacity,
settings atomic-write faults/concurrency and preservation of user/rollback data
remain covered. Host tests do not validate every receive/startup failure path.

## Exact application and static memory measurements

Default logging-off placeholder firmware, same partition layout as v0.1.0:

| Measurement | Bytes |
| --- | ---: |
| ota_0 / ota_1 application capacity, each | 1,179,648 |
| Actual EsperExperimental.bin file | **851,632** |
| Remaining headroom per app slot | **328,016** |
| Desired minimum slot headroom | 65,536 |
| LittleFS partition | 1,769,472 |
| Static DRAM total | **29,180** |
| .data / .bss | 13,348 / 15,832 |
| Static IRAM total | **89,690** |
| IRAM .text / vectors | 88,663 / 1,027 |
| Mapped flash .text / .rodata | 611,455 / 138,275 |
| idf_size aggregate estimate (not actual bin) | 853,024 |

Compared with the measured v0.1.0: binary +5,840 bytes, static DRAM +80 bytes,
static IRAM -2,252 bytes. Headroom uses the actual binary, not the map estimate.
Linker-region remaining DRAM 151,556 and IRAM 41,382 are **not runtime free heap**.

Target ABI probe, compiled only and never flashed/executed:
DNS 112, Client 64, Log_Entry 40, cJSON 40, std::string 24, byte-vector 12,
Header 12, Question 16, DnsForwarder 32 and DnsMetrics 48 bytes.

A successful from-empty build was followed by review fixes and rebuilds; the final
source then passed a separate fresh empty build/config. The table uses that final
fresh result (16 binary bytes smaller than the earlier rebuilt directory); build
metadata/path/padding differences are not a binary reproducibility guarantee.
The exact release commit must also pass the clean cloud gate.
Private builds/maps/config/toolchain copies remain outside the publication tree.
No application image, NVS/filesystem dump or merged flash binary is uploaded.

## Known bounded allocations

These are source/ABI limits, not a measured simultaneous peak heap or fragmentation
figure. Allocator capacities and SDK/stdio/lwIP/Wi-Fi/FreeRTOS overhead are additional.

| Allocation | Bound / behavior |
| --- | --- |
| DNS listener/worker stacks | 8,000 + 15,000 = 23,000 bytes; no new task |
| HTTP stack/sockets | SDK default 4,096; at most 3 open HTTP sockets |
| Receive scratch | 4,097 bytes; detects datagrams over 4,096 |
| Received packet objects | Queue 8 plus receiver/worker objects; 112-byte DNS + raw at most 4,096 each |
| Forwarder/table | 32-byte owner + 16 x 64 = 1,024 table bytes; reserved before tasks |
| Stored query payloads | At most 16 x 2,048 = 32,768 bytes, plus vector overhead/capacity |
| Pending question key | At most 259 bytes each plus string capacity; labels/type/class preserved |
| Parsed domain/question | At most 255 wire octets; escaped binary-label text can reach about 1,012 characters |
| SERVFAIL path | Temporary bounded query copies/parser/output; timeout paths can coexist with stored payloads |
| TCP retry | One worker retry; response at most 4,096 + parsed/copy scratch, total I/O deadline 3 seconds |
| Counters | 12 x 4 = 48 static bytes; no retained domains/client addresses |
| Dashboard header guard | Bounded Host 253, Origin 270 and Fetch headers 16 characters; transient buffers/copies |
| Settings | At most 4,096 + NUL input, 16 flat fields, 32-byte keys, bounded values; candidate/old JSON/temp text coexist |
| ESBL | 36 retained metadata bytes; no retained domain list/table; bounded 254-byte names/label scratch |
| Generic glob scratch | Two 128-entry LabelSpan arrays, 2,048 bytes on the existing worker stack |
| LittleFS | 512-byte read/program caches, 128-byte lookahead, plus 512 per open file and VFS/stdio overhead |
| Optional query log | Disabled by default; 100 x 40 entries + at most 100 x 254 domain storage, about 29,400 payload bytes |
| HTTP log snapshot | Further bounded copy up to about 29,400 payload bytes + per-entry JSON scratch |
| Exception pool | Configured 1,024 bytes, SDK-managed |

The startup task-notification barrier prevents a partial startup from deleting a
listener that has already allocated C++ receive objects; external vTaskDelete does
not unwind C++ locals. This reasoning is not a physical fault-injection test.
Keep query logging off until the separately authorized on-device load/soak gate.

## Synthetic C++ lookup / filesystem model

No HaGeZi data is bundled or used as a claimed real-world accuracy result here.

- 35,000 synthetic suffix rules under blocked.test; ESBL **1,225,040 bytes**.
- Reversing input order yields identical bytes.
- Actual C++ file-backed lookup: 10,000/10,000 hits; **0/10,000 negative false hits**.
- Representative final-review Windows run: validation 206.464 ms; mean hit 68.2952 us;
  miss 122.853 us. These vary with host load and are not ESP32 timings.
- Actual vendored LittleFS v2.5 core, disk format 2.0, 432 x 4,096-byte blocks:
  304 allocated = 1,245,184 bytes; 128 free = 524,288 bytes including model fixtures.
- Fault/model remount is not electrical power loss or an ESP32 VFS integration test.

## Publication review and remaining concerns

The exact 115-file source export includes hidden CI/ignore files and no symlinks,
Git metadata, hardware CAD, generated data, credentials, private paths, binaries,
backups or logs. Original MIT and vendored notices remain intact; HaGeZi remains
separate. Scanner comparisons never print private SSID/password values.

The source scan is heuristic and does not inspect arbitrary secrets, Git history
or account settings. Existing public commit metadata includes the GitHub account
email independently of source contents; it is not changed or repeated in these
docs. History rewriting or account privacy changes need a separate user decision.

Local build discovery initially failed because a child environment did not see
PowerShell PATH edits. A private process-scoped SDK/tool bootstrap resolved it;
no global settings or SDK source changed. Legacy Click/certificate-bundle warnings
are expected; final firmware source builds cleanly. Dependency Git ownership trust
is scoped only to the verified private SDK checkouts for the validation process.

## Deferred / physical acceptance

Client TCP/53, IPv6 DNS transport, DoH/DoT, caching, automatic feeds, CNAME-response
inspection, authentication, rate limiting, encrypted secrets and SDK migration
remain deferred. UDP transaction correlation is not authentication and very late
identical replies can match a reused ID/tuple.

Boot/Wi-Fi/lwIP/HTTP integration, NVS/LittleFS retention, external upstream behavior,
blocked/allowed/large/parallel probes, Power Hub path, reconnect/load pressure,
largest-free-block and heap soak with logs off/on, upgrades and real power loss
remain **pending**. No physical validation is claimed by publishing this release.
