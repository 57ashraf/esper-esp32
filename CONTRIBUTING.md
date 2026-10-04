# Contributing safely

This is an experimental, source-only ESP32-WROOM fork, not a supported household
DNS appliance. Keep changes small, memory-bounded and testable against the actual
firmware implementation. Preserve upstream notices and ESBL collision verification.

Use the pinned legacy SDK and a new placeholder-only build/config; see
[BUILD.md](docs/BUILD.md). Run Python tests, the source scanner, native tests and
Linux ASan/UBSan. Include measured binary headroom/static RAM and additional bounded
allocations when changing firmware. Do not present host timings as device results.

Never submit SSIDs, passwords, sdkconfig, settings files, serial logs, compiled
firmware, filesystem images, backups or generated blocklist data. Review your diff
and Git author/committer metadata before pushing. Use a GitHub noreply email if you
want to avoid publishing your account email. A source scan does not inspect history.

Describe the tested version, synthetic reproduction and expected behavior in an
issue or pull request. Redact domains/IPs that identify your household. Do not paste
private debug dumps; use the aggregate status counters when possible. Report security
concerns privately; [SECURITY.md](SECURITY.md) defines the limited security scope.

Do not claim physical-device validation without recording actual hardware, build,
probe commands and results. Keep device/Power Hub/upgrade/heap-soak acceptance pending
when it has not been performed. New features do not remove the trusted-LAN-only or
legacy-EOL-SDK restrictions.
