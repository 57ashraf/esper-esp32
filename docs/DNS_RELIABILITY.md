# DNS reliability design — v0.1.1

The production DNS worker uses DnsForwarder, a small worker-owned transaction table.
Only packet delivery is abstracted through ForwardTransport; host fault tests run
the same state machine as firmware. No new resolver, cache or client TCP listener
is introduced. Blocklist decisions remain in the existing worker path.

## Transaction lifecycle

Each accepted allowed query gets a randomized ID unique among the at-most-16 live
requests. Store its original wire ID, source address/port, case-folded wire question
(label boundaries/type/class retained), timestamp and at-most-2048-byte raw query.
Original request objects are not mutated. Forward raw bytes except the wire ID.

Match responses against the external resolver's exact IP/port, rewritten ID and
question key; permitted questionless DNS errors still require the source and ID.
Restore the original client ID. Out-of-order clients with identical IDs/questions
are kept separate. No live request is evicted for a newer request.

Failed upstream sends, including exceptions, remove their slot and attempt a safe
SERVFAIL. A full table attempts SERVFAIL without displacing existing requests.
Idle worker wakeups run expiry every 100 ms; requests aged at least five seconds
are removed and receive a best-effort SERVFAIL. Network delivery or memory failure
can still prevent any response. The bounded single-worker TCP retry can delay
other cleanup by up to its three-second I/O deadline.

Duplicates/late replies without a live match are discarded. IDs are not reserved
forever: a very late reply could match a later identical question if both the ID
and upstream tuple are reused. UDP DNS is not cryptographically authenticated;
the same upstream socket is reused, so this is not a hardened DNSSEC/DoH resolver.

## TCP retry and local EDNS

On an upstream TC reply, one nonblocking TCP exchange uses an absolute three-second
deadline covering connect, all partial I/O and the two-byte DNS length frame.
Transient EAGAIN/EINTR returns do not become immediate failures. EOF and lengths
outside 12–4096 bytes fail; sockets close on every exit including allocation faults.
Validate the complete reply with the same DNS parser and source/ID/question matcher.
If retry fails, preserve the safe original TC response instead of accepting a bad
reply. The client UDP/EDNS size budget still applies to the complete response.

Local blocked/error answers rebuild the question, include minimal EDNS0 OPT when
requested, clamp the advertised budget to 512–4096, do not echo options and clear
AD/DO. Zero-address/NODATA policy is unchanged. Unsupported EDNS query versions
and malformed input are still dropped; no broad compatibility claim is made.

Protocol references: [RFC 1035](https://www.rfc-editor.org/rfc/rfc1035.html),
[RFC 6891](https://www.rfc-editor.org/rfc/rfc6891.html),
[RFC 7766](https://www.rfc-editor.org/rfc/rfc7766.html).
The missing client TCP/53 service remains a known standards/large-answer limitation.

## Diagnostic and memory boundaries

Twelve relaxed atomic 32-bit counters occupy 48 static bytes, contain no domains or
client addresses, wrap modulo 2^32 and reset on reboot. A snapshot is approximate,
not a simultaneous transaction-table snapshot. See [troubleshooting](TROUBLESHOOTING.md).

The table reserves 16 entries once before tasks start. Query payload bound remains
32,768 bytes plus question strings/table/vector overhead. Queue size remains 8,
receive bound 4,097, DNS stacks 8,000 + 15,000 and TCP response bound 4,096.
No additional FreeRTOS task, cache, retained list or log buffer is added. Transient
SERVFAIL construction copies a bounded query; it is not free RAM. Status JSON adds
12 small numeric fields. Actual static sizes/headroom are in [VALIDATION.md](VALIDATION.md).
These bounds are not a measured peak device heap or fragmentation guarantee.
