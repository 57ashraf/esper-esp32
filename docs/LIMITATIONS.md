# Known v0.1.1 limitations and deliberately deferred work

- Experimental legacy EOL ESP-IDF 4.4.7; no production/availability/security warranty.
- Classic ESP32-WROOM 4 MB/no PSRAM only. Ethernet/GPIO/hardware designs excluded.
- IPv4 UDP listener only. AAAA answers can be filtered over IPv4, but IPv6 transport
  and router RDNSS/DHCPv6 bypass are not solved by this firmware.
- No client-facing TCP/53. A proper TC response may cause a standards-compliant client
  to retry TCP and fail. This affects large answers and prevents claiming complete
  DNS compatibility. Inherited external upstream TCP retry remains, with a 3-second
  absolute I/O deadline; it does not create a client TCP listener.
- Upstream TCP retry temporarily occupies the single DNS worker, so slow retries can
  fill the bounded receive queue. Its lwIP/network behavior needs hardware testing.
- Standard opcode 0, one IN-class question, maximum query 2048 bytes; receive/response
  maximum 4096 bytes. Invalid/unsupported packets are dropped (client may time out).
  Valid parsed requests that cannot forward get best-effort SERVFAIL; this is not
  a guarantee of a reply when RAM, sockets or Wi-Fi fail.
  Compression pointers must point backwards into packet data and are capped at
  32 hops/255 expanded octets. TSIG and nonzero EDNS query versions are unsupported.
- Block A/AAAA with zero addresses; CNAME/HTTPS question types with NODATA. Other
  record types are forwarded. No CNAME-response inspection, alias-chain blocking,
  DNS caching, DoH/DoT upstream, DNSSEC validation or automatic blocklist updates.
  Local blocked answers clear AD; they must not be treated as authenticated DNSSEC.
- Browser DoH, external hard-coded DNS, VPNs and cached browser/system DNS can bypass
  LAN DNS. DNS blocking cannot remove same-origin ads or distinguish URLs/paths.
- ESBL stores exact names alongside hashes; collisions cannot independently block
  an unrelated name. FNV is an index, not authentication. Structurally invalid files
  disable the index and retain the optional text overlay (**fail-open**).
  This is not a signed-data/update security design.
- Generic globs and text overlays are bounded-memory but linear scans. Keep the text
  overlay small and prefer a 20,000–50,000-name LIGHT index. No allowlist/unblock UI.
- Defaults use a directly reachable external IPv4 DNS (8.8.8.8). If the router points
  at Esper, Esper must not use the router as upstream or a DNS loop results. Direct
  public DNS may be intercepted/blocked by an ISP. DoH/DoT is deferred.
- Saved IPv4 settings retain inherited static-on-reboot behavior after a lease is
  persisted. Reserve that address outside DHCP reuse and review settings if moving
  to another network. No browser-based provisioning or configuration.
- HTTP diagnostics are unauthenticated and unencrypted. Optional query logs are
  RAM-only, capped at 100, but visible to LAN peers. Query times are device wall-clock
  values without SNTP; uptime is the reliable time diagnostic. Host/origin guards
  reduce browser rebinding, not access by LAN peers who can forge request headers.
  Aggregate counters wrap at 2^32 and are approximate, not measured latency/heap soak.
- Settings use a synchronized cJSON tree and same-directory temp/sync/rename writes;
  files are capped at 4096 bytes, 16 flat fields, 32-byte keys, and bounded string
  values. Existing configurations outside that conservative schema need a
  separately reviewed offline conversion; they are retained, not overwritten.
  host fault tests are not a physical-flash/power-cut guarantee.
- Native harness supports Windows/MinGW and POSIX Clang, with Linux ASan/UBSan CI.
  Real loopback tests cover upstream TCP/UDP helpers, not the FreeRTOS listener/queue,
  lwIP/Wi-Fi/NVS integration or a client TCP server. Fuzzing is still a smoke test,
  not exhaustive security validation. Hardware integration remains pending.
- Transaction IDs are unique while live, not permanently quarantined after reuse.
  An extremely late identical reply can match a reused ID/tuple. Source/question
  checks improve correlation but do not authenticate public UDP DNS.

Deferred: client TCP listener, SDK migration, encrypted upstream, response alias
filtering, cache, automatic feeds, allowlists, rate limiting, authentication,
encrypted secrets, IPv6 service, hardware assets, binaries/browser flasher,
device deployment. Those require separate scope and validation. Publishing the
source is authorized separately; no physical-device or router action is implied.
