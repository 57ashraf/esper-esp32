# Vendored LittleFS dependency

Minimal source snapshot of esp_littlefs v1.4.1 (MIT), commit
4a5121096bea32ac908735d971cffd34e5fe280f, and littlefs core v2.5
(BSD-3-Clause), commit 40dba4a556e0d81dfbe64301a6aa4e18ceca896c.
Original license files are retained. See repository NOTICE.md.

No examples, Git metadata, component-manager manifest or mklittlefs binary is
included. Firmware uses disk format 2.0; acquire mklittlefs 2.5.1-2 separately
for deliberate first-install images. Automatic formatting on mount failure is disabled
by the application; upgrading must not overwrite LittleFS/NVS.
