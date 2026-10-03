# Source-only v0.1.0 release checklist

Completed local preparation gates are recorded in VALIDATION.md, not assumed here.

- [ ] Read experimental/EOL/LAN-only/security caveats and accept the scope.
- [ ] Preserve MIT Esper attribution and vendored MIT/BSD licenses; review NOTICE.
- [ ] Confirm no hardware designs or HaGeZi/generated data are in the export.
- [ ] Use a fresh exact ESP-IDF v4.4.7 build with placeholder credentials.
- [ ] Record actual binary, app slots/headroom, static RAM and bounded allocations.
- [ ] Run actual C++ host tests, collision/corruption/reproducibility tests and scan.
- [ ] Review exact publication tree and any future Git history for private material.
- [ ] Verify dashboard contains no secret, mutation or arbitrary filesystem route.
- [ ] Leave on-device/Power Hub/heap-soak/power-cut/upgrade validation explicitly pending.
- [x] User selected repository name `esper-esp32`, version/tag `v0.1.0` and
  GitHub-only attribution to `@57ashraf`; no legal name or email is required.
- [ ] Obtain separate authorization before creating the repository/tag or publishing.
- [ ] Keep local credentials, backups, builds and compiler tools outside the export.
- [ ] If/when authorized, create a new source-only repository; never import original
  history or use git add on the original working/device project.
- [ ] Review any CI run separately; no artifact-upload or deployment steps.
- [ ] Do not call this a stable or hardware-validated release.

No GitHub repository, commit, tag, push, remote CI run or deployment is authorized
by the local preparation request. Stop at the reviewed publication copy.
