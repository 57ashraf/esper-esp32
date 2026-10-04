# Public source-only tree — v0.1.1

Repository: `esper-esp32`; GitHub maintainer: `@57ashraf`.
Exactly 115 source files, including hidden CI/ignore files. This is the separate
publication copy, not the original working project/history. No hardware, private
configuration, credentials, generated lists, binaries, backups or deployment data.

```text
esper-esp32/
├── .github/
│   └── workflows/
│       └── ci.yml
├── docs/
│   ├── BUILD.md
│   ├── DNS_RELIABILITY.md
│   ├── INSTALL.md
│   ├── LIMITATIONS.md
│   ├── PUBLICATION_TREE.md
│   ├── RELEASE_CHECKLIST.md
│   ├── TROUBLESHOOTING.md
│   └── VALIDATION.md
├── examples/
│   ├── sdkconfig.example
│   └── settings.example.json
├── software/
│   ├── firmware/
│   │   ├── components/
│   │   │   ├── dns/
│   │   │   │   ├── include/
│   │   │   │   │   └── dns/
│   │   │   │   │       ├── dns.h
│   │   │   │   │       ├── forwarder.h
│   │   │   │   │       ├── logging.h
│   │   │   │   │       ├── metrics.h
│   │   │   │   │       ├── server.h
│   │   │   │   │       └── tcp_transport.h
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── dns.cpp
│   │   │   │   ├── forwarder.cpp
│   │   │   │   ├── logging.cpp
│   │   │   │   ├── metrics.cpp
│   │   │   │   ├── server.cpp
│   │   │   │   └── tcp_transport.cpp
│   │   │   ├── error/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── error.cpp
│   │   │   │   └── error.h
│   │   │   ├── events/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── events.cpp
│   │   │   │   └── events.h
│   │   │   ├── flash/
│   │   │   │   ├── files/
│   │   │   │   │   ├── app_scripts.js
│   │   │   │   │   ├── defaultsettings.json
│   │   │   │   │   ├── homepage.html
│   │   │   │   │   └── stylesheet.css
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── filesystem.cpp
│   │   │   │   └── filesystem.h
│   │   │   ├── http/
│   │   │   │   ├── include/
│   │   │   │   │   ├── get_handlers.h
│   │   │   │   │   ├── safe_dashboard.h
│   │   │   │   │   └── webserver.h
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── get_handlers.cpp
│   │   │   │   ├── safe_dashboard.cpp
│   │   │   │   └── webserver.cpp
│   │   │   ├── lists/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── lists.cpp
│   │   │   │   └── lists.h
│   │   │   ├── littlefs/
│   │   │   │   ├── include/
│   │   │   │   │   └── esp_littlefs.h
│   │   │   │   ├── src/
│   │   │   │   │   ├── littlefs/
│   │   │   │   │   │   ├── lfs_util.c
│   │   │   │   │   │   ├── lfs_util.h
│   │   │   │   │   │   ├── lfs.c
│   │   │   │   │   │   ├── lfs.h
│   │   │   │   │   │   └── LICENSE.md
│   │   │   │   │   ├── esp_littlefs.c
│   │   │   │   │   ├── fd_guard.h
│   │   │   │   │   ├── lfs_config.c
│   │   │   │   │   ├── lfs_config.h
│   │   │   │   │   ├── littlefs_api.c
│   │   │   │   │   └── littlefs_api.h
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── Kconfig
│   │   │   │   ├── LICENSE
│   │   │   │   └── README.md
│   │   │   ├── netif/
│   │   │   │   ├── include/
│   │   │   │   │   ├── ip.h
│   │   │   │   │   └── wifi.h
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── ip.cpp
│   │   │   │   └── wifi.cpp
│   │   │   ├── settings/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── settings.cpp
│   │   │   │   └── settings.h
│   │   │   └── README.md
│   │   ├── main/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── Kconfig.projbuild
│   │   │   └── startup.cpp
│   │   ├── CMakeLists.txt
│   │   ├── partitions_table.csv
│   │   ├── README.md
│   │   └── sdkconfig.defaults
│   └── tools/
│       └── blocklist/
│           ├── benchmark_blocklist.py
│           ├── blocklist_format.py
│           ├── generate_blocklist.py
│           ├── README.md
│           └── test_blocklist.py
├── tests/
│   ├── shims/
│   │   ├── freertos/
│   │   │   ├── FreeRTOS.h
│   │   │   ├── semphr.h
│   │   │   └── task.h
│   │   ├── lwip/
│   │   │   └── sockets.h
│   │   ├── esp_http_server.h
│   │   ├── esp_log.h
│   │   ├── esp_system.h
│   │   └── esp_timer.h
│   ├── vendor/
│   │   └── cJSON/
│   │       ├── cJSON.c
│   │       ├── cJSON.h
│   │       └── LICENSE
│   ├── host_tests.cpp
│   ├── littlefs_tests.cpp
│   ├── README.md
│   ├── run_host_tests.py
│   ├── test_release.py
│   ├── test_tools.py
│   └── transport_tests.cpp
├── tools/
│   ├── abi_sizes.cpp
│   ├── measure_abi.py
│   ├── prepare_filesystem.py
│   ├── report_build.py
│   └── scan_publication.py
├── .gitignore
├── .publicationignore
├── CHANGELOG.md
├── CONTRIBUTING.md
├── LICENSE
├── NOTICE.md
├── README.md
└── SECURITY.md
```

Publication is restricted to this exact export. Private validation/build/tool files
and the surrounding workspace must never be uploaded. tests/test_release.py checks
this entire documented tree against the export.
