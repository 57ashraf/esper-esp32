# ESBLv1 tooling — separately obtained blocklist data

No HaGeZi data or generated ESBL is bundled. Obtain **HaGeZi Multi LIGHT** from
[the project's domain-format list](https://github.com/hagezi/dns-blocklists/blob/main/domains/light.txt).
Review its [license](https://github.com/hagezi/dns-blocklists/blob/main/LICENSE),
disclaimer and upstream list changes yourself. Keep the downloaded feed and its
license/provenance privately. Do not assume generated data is covered by this fork's MIT license.

Prefer a roughly **20,000–50,000-domain** LIGHT list, not maximum capacity. Actual
feed size changes. Retain at least 10% filesystem payload margin for metadata,
settings atomic replacement and recovery; raw ESBL fit alone is not an image-fit
guarantee. First-install image generation must also succeed at 1728 KiB.

From the repository root, save the external list under ignored private/, then:

```text
python -B software/tools/blocklist/generate_blocklist.py private/light.txt private/blocklist.bin
python -B software/tools/blocklist/benchmark_blocklist.py private/blocklist.bin --queries 10000
python -B -m unittest discover -s software/tools/blocklist -p test_blocklist.py -v
```

See docs/INSTALL.md before installing any filesystem data. These tools do not
download data, update the running device, alter network settings or flash anything.
Generation refuses an existing output (keep it as rollback data), defaults to
50,000 rules maximum, and refuses payloads exceeding 90% of the stated capacity.
It fsyncs a temporary file then publishes it with an exclusive hard link; host
filesystems without hard-link support fail safely rather than overwriting a file.

## Matching and format

ASCII case folds to lower case; presentation trailing dots are removed.
Bare domains match the apex and descendants at label boundaries.
*.example.org matches descendants only, not the apex. Generic * and ? operate
within labels; legacy multi-label patterns match contiguous label sequences.
A and AAAA are zero-address blocked; CNAME and HTTPS questions are NODATA blocked.
Allowed responses are not inspected for CNAME targets.

ESBLv1: 40-byte little-endian header, sorted 16-byte FNV-1a-64 index records,
exact canonical name bytes, then optional generic wildcard records. Index flags
distinguish suffix/descendants-only rules. Lookup lower-bounds the hash and
byte-compares every equal-hash candidate; no hash-only blocking.

Firmware startup validates bounds, offsets, ordering, flags/reserved bytes,
canonical strings and their hashes without retaining the list in RAM.
Corruption disables the index and logs a generic diagnostic, preserving data.
Lookup retains small metadata only, reopens LittleFS as needed and uses bounded
name/label scratch. Large text overlays/generic globs are slower linear scans.

Python tooling tests are supplemented by native tests linking the actual C++
firmware matcher, including constant-hash adversarial collisions and corruption.
Benchmarks on a host filesystem are not ESP32 latency/heap measurements.
No cache, automatic feed updates or response-CNAME inspection is implemented.
