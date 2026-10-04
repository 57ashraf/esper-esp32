# Experimental source-only release checklist

Use [VALIDATION.md](VALIDATION.md) for measured evidence and release notes for the
exact source commit and CI run. Never infer hardware validation from a green build.

- [ ] Preserve upstream MIT attribution and every vendored MIT/BSD notice.
- [ ] Keep the release experimental, source-only and trusted-LAN-only.
- [ ] Prominently state that this release has not been physically validated.
- [ ] Preserve the legacy EOL SDK and no-public-HTTP/DNS warnings.
- [ ] Test actual C++ logic, transport faults, collisions, storage and secret routes.
- [ ] Run Windows tests and Linux ASan/UBSan on the release candidate.
- [ ] Build from an empty directory/config with exact ESP-IDF v4.4.7 and placeholders.
- [ ] Record actual application binary, slot headroom, static RAM and allocation bounds.
- [ ] Scan the exact export, including hidden CI files and known-private-value comparison.
- [ ] Check licenses and privacy in new files, Git diffs and commit metadata/history.
- [ ] Ensure no hardware designs, credentials, private paths, logs, binaries, build
  outputs, backups, generated blocklists or surrounding workspace files are included.
- [ ] Verify the documented publication tree matches the exact source export.
- [ ] Check current commit's CI jobs, not just an old release or the branch badge.
- [ ] Publish only after the user explicitly authorizes source publication.
- [ ] Create a new experimental prerelease tag without moving older tags.
- [ ] Verify public files by path/blob hash and tag-to-commit correspondence.
- [ ] Attach no custom release artifacts; automatic archives contain source only.
- [ ] Leave boot, Wi-Fi/lwIP integration, heap soak, upgrade/power loss and Power Hub
  regression tests pending until separately authorized and actually performed.

The v0.1.1 improvement/publication request authorizes this source release only.
It does not authorize a hardware flash or network-setting change. The original
working project, prior local publication copy and v0.1.0 tag must remain intact.
