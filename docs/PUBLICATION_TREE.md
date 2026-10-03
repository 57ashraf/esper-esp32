# Proposed public source-only tree — v0.1.0

Repository name: `esper-esp32`; GitHub maintainer: `@57ashraf`.
This is the independent publication copy, not the original project/history.
No repository or tag has been created. No hardware, credentials, generated
blocklists, binaries or deployment data are included. The tree below lists all
105 files, including hidden CI and ignore files.

```text
esper-esp32/
├── .github/
│   └── workflows/
│       └── ci.yml
├── .gitignore
├── .publicationignore
├── CHANGELOG.md
├── LICENSE
├── NOTICE.md
├── README.md
├── SECURITY.md
├── docs/
│   ├── BUILD.md
│   ├── INSTALL.md
│   ├── LIMITATIONS.md
│   ├── PUBLICATION_TREE.md
│   ├── RELEASE_CHECKLIST.md
│   └── VALIDATION.md
├── examples/
│   ├── sdkconfig.example
│   └── settings.example.json
├── software/
│   ├── firmware/
│   │   ├── CMakeLists.txt
│   │   ├── README.md
│   │   ├── components/
│   │   │   ├── README.md
│   │   │   ├── dns/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── dns.cpp
│   │   │   │   ├── include/
│   │   │   │   │   └── dns/
│   │   │   │   │       ├── dns.h
│   │   │   │   │       ├── logging.h
│   │   │   │   │       └── server.h
│   │   │   │   ├── logging.cpp
│   │   │   │   └── server.cpp
│   │   │   ├── error/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── error.cpp
│   │   │   │   └── error.h
│   │   │   ├── events/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── events.cpp
│   │   │   │   └── events.h
│   │   │   ├── flash/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── files/
│   │   │   │   │   ├── app_scripts.js
│   │   │   │   │   ├── defaultsettings.json
│   │   │   │   │   ├── homepage.html
│   │   │   │   │   └── stylesheet.css
│   │   │   │   ├── filesystem.cpp
│   │   │   │   └── filesystem.h
│   │   │   ├── http/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── get_handlers.cpp
│   │   │   │   ├── include/
│   │   │   │   │   ├── get_handlers.h
│   │   │   │   │   ├── safe_dashboard.h
│   │   │   │   │   └── webserver.h
│   │   │   │   ├── safe_dashboard.cpp
│   │   │   │   └── webserver.cpp
│   │   │   ├── lists/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── lists.cpp
│   │   │   │   └── lists.h
│   │   │   ├── littlefs/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── Kconfig
│   │   │   │   ├── LICENSE
│   │   │   │   ├── README.md
│   │   │   │   ├── include/
│   │   │   │   │   └── esp_littlefs.h
│   │   │   │   └── src/
│   │   │   │       ├── esp_littlefs.c
│   │   │   │       ├── fd_guard.h
│   │   │   │       ├── lfs_config.c
│   │   │   │       ├── lfs_config.h
│   │   │   │       ├── littlefs/
│   │   │   │       │   ├── LICENSE.md
│   │   │   │       │   ├── lfs.c
│   │   │   │       │   ├── lfs.h
│   │   │   │       │   ├── lfs_util.c
│   │   │   │       │   └── lfs_util.h
│   │   │   │       ├── littlefs_api.c
│   │   │   │       └── littlefs_api.h
│   │   │   ├── netif/
│   │   │   │   ├── CMakeLists.txt
│   │   │   │   ├── include/
│   │   │   │   │   ├── ip.h
│   │   │   │   │   └── wifi.h
│   │   │   │   ├── ip.cpp
│   │   │   │   └── wifi.cpp
│   │   │   └── settings/
│   │   │       ├── CMakeLists.txt
│   │   │       ├── settings.cpp
│   │   │       └── settings.h
│   │   ├── main/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── Kconfig.projbuild
│   │   │   └── startup.cpp
│   │   ├── partitions_table.csv
│   │   └── sdkconfig.defaults
│   └── tools/
│       └── blocklist/
│           ├── README.md
│           ├── benchmark_blocklist.py
│           ├── blocklist_format.py
│           ├── generate_blocklist.py
│           └── test_blocklist.py
├── tests/
│   ├── README.md
│   ├── host_tests.cpp
│   ├── littlefs_tests.cpp
│   ├── run_host_tests.py
│   ├── shims/
│   │   ├── esp_http_server.h
│   │   ├── esp_log.h
│   │   ├── esp_system.h
│   │   ├── esp_timer.h
│   │   ├── freertos/
│   │   │   ├── FreeRTOS.h
│   │   │   ├── semphr.h
│   │   │   └── task.h
│   │   └── lwip/
│   │       └── sockets.h
│   ├── test_release.py
│   ├── test_tools.py
│   └── vendor/
│       └── cJSON/
│           ├── LICENSE
│           ├── cJSON.c
│           └── cJSON.h
└── tools/
    ├── abi_sizes.cpp
    ├── measure_abi.py
    ├── prepare_filesystem.py
    ├── report_build.py
    └── scan_publication.py
```
