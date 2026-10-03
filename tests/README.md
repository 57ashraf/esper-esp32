# Validation scope

Run the commands in [BUILD.md](../docs/BUILD.md). Native C++ tests compile the actual
firmware DNS parser/serializer, transaction matcher/ID allocator, ESBL loader/matcher,
settings implementation, filesystem wrapper, optional query log, safe dashboard
route/JSON logic and actual HTTP registration/GET handlers. Only platform interfaces
(FreeRTOS locks, sockets, time, HTTP transport) and embedded asset bytes are shimmed.

The socket shim captures sendto payloads. It does not validate lwIP scheduling,
physical flash power loss, Wi-Fi, HTTP server integration or the Power Hub. CI is
defined but has not been run on GitHub for this source-only local copy.

Fixtures are generated in a temporary directory; no public blocklist is bundled.
A logging-disabled executable verifies the default privacy mode. A 35,000-domain
synthetic benchmark runs the actual C++ filesystem-backed lookup with 10,000 hits
and 10,000 negative probes, and checks generator order reproducibility.
A second executable injects a constant index hash in the actual firmware code
under a host-test-only macro. Name byte verification must still reject false hits.
This macro must never be defined in firmware builds.

Malformed input fuzzing is a bounded deterministic smoke test, not a comprehensive
fuzzer or a security proof. Windows/MinGW is the native harness platform for v0.1.0.
Host lookup timings are not ESP32 performance or on-device heap measurements.
The actual vendored littlefs core also runs on a bounded host RAM-flash model:
filesystem fit, failed temp sync, atomic replacement and remount are tested.
This model does not simulate an ESP32 VFS or real electrical power loss.
