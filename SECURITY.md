# Security scope

Experimental trusted-LAN software on an EOL SDK. No production-security claim,
authentication, HTTPS dashboard, encrypted DNS upstream, rate limiting or DNSSEC
validation. Never expose HTTP/DNS publicly. Restrict access with the network,
not by assuming the device implements a firewall.

Only embedded dashboard assets and safe diagnostic JSON are served. Settings,
blocklist files and filesystem paths are never HTTP resources. No mutation routes
are registered. Query logging defaults off; enabling it exposes up to 100 recent
domains/types/times to LAN peers. Client addresses are omitted.

Local Wi-Fi credentials are embedded in private firmware/configuration and saved
in LittleFS/NVS. Do not publish binaries, flash images, logs, backups, edited
sdkconfig or settings files. Flash encryption is not configured by this release.
No software-only secret scanner can prove that arbitrary prose contains no secrets.

Use tools/scan_publication.py on the exact export. Optionally compare against a
private sdkconfig without printing its values. Also review all files and the Git
history before any publication. This local export has no copied Git history.
Report issues privately through the maintainer's chosen channel; no invented
security-contact address or response-time promise is provided.
