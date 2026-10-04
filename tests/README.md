# What the tests actually validate

From the repository root, follow [BUILD.md](../docs/BUILD.md).
All fixtures are synthetic and temporary; no blocklist feed or real credentials
are included. Nothing in these suites accesses a COM port, flashes hardware,
changes network settings or queries an external resolver.

## Actual firmware logic

The native suites compile the shipped DNS parser/serializer, DnsForwarder state
machine, transaction matcher/ID allocator, ESBL loader/matcher, settings, filesystem
wrapper, optional query log and actual dashboard route/JSON/HTTP handlers.

The forwarding seam replaces only network delivery and time inputs, not the state
machine. Tests cover concurrent client-ID collisions, original-ID restoration,
source/type mismatch, duplicates and late replies, 16-entry overload, idle expiry,
send exceptions/failures, client delivery failure and validated/failing TCP retry.
They preserve byte-for-byte allowed A/AAAA/HTTPS payloads and zero-address/NODATA
blocking semantics. EDNS local replies and SERVFAIL are parsed again by firmware.

FreeRTOS locks, HTTP transport and embedded assets are shimmed. The receive task,
FreeRTOS queues, Wi-Fi, NVS and ESP-IDF/lwIP scheduling are **not** host integration
tests. Socket capture is not actual UDP server validation.

## Real host transport

A separate executable compiles the actual firmware tcp_transport.cpp and UDP send
helper against real Winsock/POSIX loopback sockets. It binds only 127.0.0.1 and
ephemeral ports, never port 53. Cases include fragmented length/body frames, 4096-byte
payloads, short prefix/body EOF, zero/undersized/oversized length, stalled peers,
refused connections, input bounds and the separate ephemeral UDP source port.

These are upstream transport tests, not a client TCP/53 listener or hardware proof.
Host timing checks use generous bounds; they are not performance benchmarks.

## Blocklists and storage

Normal, forced-hash-collision and query-log-disabled variants exercise the actual
firmware. A firmware-build guard rejects the host-only collision hash override.
The synthetic 35,000-name benchmark checks reproducibility, 10,000 hits and 10,000
negative probes through actual filesystem-backed C++ lookup. It does not measure
HaGeZi quality, ESP32 flash latency or peak device RAM.

The vendored LittleFS core runs on a RAM-flash model: fit, sync failure, atomic
replacement and remount. Settings tests cover malformed input, concurrency,
candidate/temp/sync/rename faults and preservation of current/rollback data.
This is not a physical electrical power-cut or ESP32 VFS test.

## Windows, POSIX and sanitizers

The harness supports Windows/MinGW and POSIX Clang. Normal commands:

~~~text
python -B tests/run_host_tests.py
python -B -m unittest discover -s software/tools/blocklist -p test_blocklist.py -v
python -B -m unittest discover -s tests -p "test_*.py" -v
python -B tools/scan_publication.py
~~~

On Linux with Clang, the same C++ and LittleFS/loopback tests can run under ASan/UBSan:

~~~bash
CC=clang CXX=clang++ ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
python3 -B tests/run_host_tests.py --sanitizers
~~~

CI uses both host platforms plus the pinned SDK placeholder build and publishes no
artifacts. Sanitizers do not prove freedom from data races; the test counter itself
is atomic and production log/settings locking is covered by concurrency checks.
Malformed fuzzing is a deterministic 20,000-input smoke per mode, not an exhaustive
fuzzer or security proof. Physical-device, heap-soak and Power Hub checks stay pending.
