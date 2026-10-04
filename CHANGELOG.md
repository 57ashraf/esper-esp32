# Changelog

## v0.1.1 — 2026-10-04 — experimental, source-only

### Reliability

- Extracted the existing allowed-query transaction lifecycle into DnsForwarder,
  used by the production worker and actual-C++ fault tests. No new resolver/cache.
- Kept 16 live requests, 8 received packets, 2048-byte stored queries, source/question
  checks, unique live IDs, original-ID restoration and separate upstream UDP socket.
- On send failure/exception, release the slot and attempt SERVFAIL. On full-table
  overload, attempt SERVFAIL without evicting a live request.
- Run timeout cleanup when idle, not just on later traffic; best-effort SERVFAIL
  and bounded slot removal at the five-second age threshold.
- Reworked the existing upstream-only TCP retry into a tested transport helper.
  One nonblocking three-second connect/I/O deadline, partial framing, transient
  EAGAIN/EINTR handling, 12–4096-byte response bounds and exception-safe socket close.
- Accept only validated complete TCP replies matching the live request; retain
  the safe UDP TC response on failure. No client-facing TCP/53 added.
- Acknowledge EDNS0 in local zero-address/NODATA/SERVFAIL replies, without echoing
  client options or asserting AD/DO. Rebuild questions and error responses safely.
- Check initialization resources before starting tasks and clean up sockets,
  queue and queued objects when startup fails.

### Safe diagnostics and tests

- Added twelve atomic, domain/client-free diagnostic counters, even with logs off.
- Added a dashboard Host/Origin/Fetch-Site guard against browser rebinding and
  foreign-origin data reads; allow legitimate top-level navigation to the home page.
  This is not authentication, encryption or protection against a hostile LAN peer.
- Added forwarding fault tests and real Winsock/POSIX loopback TCP/UDP tests,
  including dribbling peers that cannot extend the absolute deadline.
- Made the same C++/LittleFS harness portable to POSIX; added Linux ASan/UBSan CI,
  keeping Windows and exact-SDK placeholder build gates. Made the test counter atomic.
- CI now verifies the SDK source commit, not only its numeric version.
- Added contribution, design and read-only troubleshooting guidance, updated release
  identity and removed stale pre-publication claims from current documentation.

ESBLv1, exact-byte collision verification, matching/blocking semantics, saved private
configuration, partition layout and original license notices are unchanged.
No device probed/flashed or PC/router/network setting changed. Physical acceptance
remains pending. No binaries, hardware designs, generated lists or credentials bundled.

## v0.1.0 — 2026-10-03 — initial experimental source-only publication

- Independent software-only export; preserved Esper MIT and vendored MIT/BSD notices.
  Unofficial fork maintained under GitHub @57ashraf; no hardware designs/feed data.
- Pinned legacy EOL ESP-IDF 4.4.7 / Xtensa GCC 8.4.0, minimal LittleFS and cJSON
  source/licences. Explicit experimental, trusted-LAN-only and unvalidated warnings.
- Replaced web configuration/edit/restart/OTA/filesystem routes with embedded read-only
  assets, safe status JSON, cJSON escaping, textContent and restrictive HTTP headers.
- Disabled query logging by default; synchronized 100-entry logs/snapshots and removed
  credential logging, including closed-library Wi-Fi info output.
- Hardened bounded settings/schema parsing, move-only filesystem wrappers, synchronized
  candidate/temp/sync/atomic replacement, descriptor bounds and write/close faults.
  No automatic filesystem formatting, NVS erasure or destructive data migration.
- Preserved canonical label-aware suffix/glob and collision-safe flash ESBL semantics;
  zero-address A/AAAA and NODATA CNAME/HTTPS decisions; raw allowed record forwarding.
- Bounded DNS integers/names/compression/RRs/OPT and malformed inputs; source/question
  correlation, unique pending IDs, receive/client caps and UDP-budget-safe TC responses.
- Preserved external ephemeral upstream UDP and inherited upstream TCP retry.
  Client TCP/53, DoH/DoT, cache, auto updates and CNAME-response inspection deferred.
- Added actual firmware host tests, deterministic fuzz smoke, forced collisions,
  corruption/reproducibility/capacity checks, LittleFS model and synthetic benchmark.
- Added placeholder examples, ignore/source/secret scans, build/install/upgrade/security
  docs, exact-tree checks, validation report and minimal-permission CI without artifacts.
- Both public push/tag CI runs passed. Physical board, heap soak, upgrade/power-cut and
  Power Hub tests were not performed for the hardened source build.
