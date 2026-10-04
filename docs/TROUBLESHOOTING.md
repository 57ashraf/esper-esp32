# Read-only troubleshooting

Device validation remains pending for this release. Start with direct tests on one
device; do not change household DNS until direct tests and a separately authorized
physical validation pass. ESP_LAN_IP below is a placeholder for your device address.

## Direct DNS on Windows

These commands select a server explicitly; they do not change the PC's DNS settings:

```text
nslookup -type=A ad.doubleclick.net ESP_LAN_IP
nslookup -type=AAAA ad.doubleclick.net ESP_LAN_IP
nslookup -type=A example.com ESP_LAN_IP
nslookup -type=AAAA example.com ESP_LAN_IP
```

If the installed list contains the ad domain and blocking is enabled, its A/AAAA
answer should be 0.0.0.0/::. A normal domain must return an allowed response, not
merely time out. HTTPS type 65 and EDNS probes need a client that supports them
(for example `dig @ESP_LAN_IP example.com TYPE65 +bufsize=1232`). This project does
not install such tools or alter PC/router settings. Client TCP/53 is not provided.

No index loaded? Inspect `indexed`, `records` and `blocklist_bytes` in /status.json.
Missing/corrupt ESBL fails open with the optional legacy text overlay. Install a
separately generated list only through a deliberate backed-up local workflow.

## Counters without query logging

Open `http://ESP_LAN_IP/` or /status.json. Query logging can stay disabled. Compare
counter deltas immediately before and after a probe, allowing for other LAN traffic:

| Counter | Meaning / useful clue |
| --- | --- |
| dns_received | Datagrams read by the listener, queries and upstream replies |
| dns_malformed | Bad size/source/header/direction/parse or receive-path allocation failure |
| dns_queue_drops | Receive queue full; a valid query gets best-effort SERVFAIL |
| dns_blocked | Blocked decisions, not guaranteed delivery or unique domains |
| dns_forwarded / dns_answered | Successfully sent upstream queries / delivered matching answers |
| dns_unmatched | Wrong source/question/ID, duplicate or late reply without a live match |
| dns_overloaded | All 16 transaction slots were occupied |
| dns_send_failures | Forwarder delivery failures, including exceptions |
| dns_timeouts | Pending requests expired; best-effort SERVFAIL attempted |
| dns_tcp_attempts / dns_tcp_failures | Upstream TC retries / no validated complete reply |

Counters are approximate live totals, wrap at 2^32 and reset on reboot. They do not
measure packet latency, unique clients, all SDK errors or runtime peak heap.
`free_heap`/`min_heap` are live SDK diagnostics, not release soak-test evidence.

## Dashboard says Forbidden

Use the current IPv4 address or configured local hostname, port 80. Foreign Host
headers, cross-origin reads, unexpected ports and missing Host are rejected to
reduce DNS rebinding. A custom reverse-proxy name needs separate review; it is not
allowlisted automatically. The guard is not authentication: LAN peers can forge
headers. Do not weaken it to expose the dashboard publicly.

## Router loops and bypass

If a router forwards DNS to Esper, Esper's upstream must be an independent reachable
external resolver, not that router. Otherwise allowed queries form a loop. No router
setting is changed by this project. Validate explicit direct blocked and allowed
queries before manually testing a proxy path. Router IPv6 RDNSS/DHCPv6, browser DoH,
hard-coded DNS, VPNs and caches can bypass IPv4 DNS blocking.

If an ISP blocks/intercepts public DNS, this release has no DoH/DoT escape path.
Do not claim router compatibility based on host mocks or previous device firmware.

Before reporting issues, run the exact-source tests/build and include safe counter
deltas/version only. Never paste private configs, Wi-Fi values, full serial dumps,
generated lists or flash backups into GitHub issues.
