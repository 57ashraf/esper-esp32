# v0.1.0 — experimental source-only local release preparation

## Distribution and scope

- Created an independent software-only export, without original Git history.
- Retained Esper MIT attribution and unofficial-fork notice; separated inherited,
  earlier fork and v0.1.0 work in NOTICE.md.
- Adopted the user-selected repository name esper-esp32 and version/tag v0.1.0;
  credited GitHub @57ashraf without a legal name or email. No repository/tag created.
- Made pending physical-device validation prominent before the README features.
- Pinned legacy EOL ESP-IDF v4.4.7 / Xtensa GCC 8.4.0, vendored minimum LittleFS
  source/licenses, and vendored matching cJSON source/license for host tests.
- Excluded hardware CAD, GPIO/Ethernet application components, generated data,
  compiled artifacts, credential/configuration files, backups and deployment copies.
- No HaGeZi data is redistributed; documented separate acquisition/local conversion.

## Read-only diagnostics and privacy

- Replaced inherited dashboard with embedded read-only LAN assets.
- Removed web settings/configuration, blocklist edits, restart, OTA/update checks,
  provisioning pages and arbitrary filesystem serving.
- Replaced /settings.json with allowlisted safe /status.json.
- Restricted routes to exact embedded assets, status and optional query log.
- Added no-store, nosniff and restrictive Content-Security-Policy headers.
- Used cJSON escaping and DOM textContent; omitted client IPs from query output.
- Disabled query logging by default; locked 100-entry retention and snapshots;
  corrected record types and empty-array output.
- Removed Wi-Fi value logging and suppressed closed-library Wi-Fi info output.

## Storage and startup

- Made application file wrappers move-only and null-safe; checked short reads,
  writes, flush, fsync and close errors; rejected invalid relative/traversal paths.
- Bounded settings files/schema, rejected duplicate keys, trailing data, NUL and
  recursive/nested values; synchronized reads/writes and freed cJSON print buffers.
- Implemented candidate-copy/temp-file/sync/close/atomic-rename commits; RAM and
  previous disk state survive failed writes.
- Disabled automatic filesystem formatting and automatic NVS erasure.
- Removed destructive file migration; retain current/previous data directories in
  place, stop on incomplete/corrupt data, and require deliberate recovery.
- Added LittleFS descriptor bounds/null guards, close-failure FD cleanup and locked
  write-sync handling; no core format change.
- Made Wi-Fi buffer copies bounded, checked setup errors and waited for a LAN IP
  before service startup; preserved saved IPv4/static-on-reboot semantics.
- Removed automatic update/rollback orchestration; documented explicit manual
  upgrade/backup requirements and shared-data rollback caveat.

## DNS and blocklists

- Preserved canonical case/trailing-dot/label-aware suffix and wildcard semantics;
  preserved A/AAAA zero-address and CNAME/HTTPS NODATA blocking.
- Preserved raw allowed-record payloads, external upstream socket separation and
  inherited upstream TCP retry; did not add client-facing TCP/53.
- Validated query/header/class/count/packet bounds, DNS name lengths and backwards
  14-bit compression pointers with bounded hops; rebuilt local questions safely.
- Validated RR boundaries and OPT owners, counts, version and option lengths;
  rejected malformed/unsupported packets without unsafe allocations.
- Used unique randomized outstanding upstream IDs, restored original client IDs
  and matched source IP/port plus case-folded wire question/type/class.
- Capped outstanding clients/receive queue, avoided evicting live transactions,
  rejected wrong-socket query/response direction and detected oversized datagrams.
- Respected 512/EDNS client UDP budgets; generated safe TC question responses for
  oversized upstream results; bounded upstream TCP I/O with an absolute deadline.
- Corrected local hostname AAAA handling so it no longer fabricates an A answer.
- Strengthened ESBL bounds/flags/reserved/hash/canonical-string validation without
  loading list contents into RAM or changing ESBLv1 format.
- Kept exact-byte collision verification, including equal-hash candidate walking.
- Rejected the host-test-only collision hash override in firmware builds.
- Skipped overlong text-rule fragments; removed runtime blocklist editing APIs.
- Yielded during full index startup validation.
- Added non-overwriting, fsync/atomic-publish generation and conservative host
  rule-count/payload budgets; preserved previous generated outputs.

## Evidence and release workflow

- Added native tests linking actual C++ parser/serializer, transaction helpers,
  matcher/index verifier, storage/settings/logging and HTTP logic.
- Added malformed DNS fuzz smoke, compression/EDNS/record-type/collision/corruption,
  write-failure/concurrency, read-only routes/secret-output tests and a real
  filesystem-backed C++ synthetic lookup benchmark.
- Added generator reproducibility/capacity/overwrite/corruption and first-install
  tooling tests, publication/DOM/source/secret scans and optional known-secret checks.
- Added blank examples, ignore/export rules, bootstrap/build/install/upgrade docs,
  limitations/security guidance, release checklist and validation report.
- Corrected the full publication tree and added an exact-tree documentation test;
  tightened SDK configuration, settings, log, environment and archive exclusions.
- Added minimal-permission CI with checksum-pinned host compiler and SDK-pinned
  placeholder build; no firmware artifact publishing or deployment.
- Measured actual target ABI sizes and a from-empty placeholder build.
- Did not flash, change network settings, create a repository, commit, push, publish
  or claim on-device/Power Hub/heap-soak/power-loss validation.
