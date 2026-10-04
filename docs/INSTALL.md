# First install versus upgrade

These are **manual future-use instructions**, not operations performed on the device
during source release work. v0.1.1 hardware validation is pending. Verify your exact
ESP32 variant and 4 MB flash before any flash. ESP32-S2/S3/C3 and PSRAM boards are not
validated targets. Save the complete original flash/config privately before upgrading.

## First install on a deliberately blank/new device

1. Configure Wi-Fi locally with menuconfig and build using docs/BUILD.md.
2. Obtain a blocklist separately; generate private/blocklist.bin using the list guide.
3. Prepare a NEW private input directory from the local sdkconfig:

```text
python -B tools/prepare_filesystem.py --sdkconfig private/sdkconfig --blocklist private/blocklist.bin --output private/first-install-fs
```

This creates ota_0/settings.json, blacklist.txt (empty overlay) and blocklist.bin.
It contains credentials. The tool refuses an existing output directory.
Keep this directory/image private, out of Git and release uploads.

4. Obtain the separately licensed mklittlefs 2.5.1-2 tool from
[its release](https://github.com/earlephilhower/mklittlefs/releases/tag/2.5.1-2).
Check its release provenance before running it. Generate a format-2.0 image:

```text
mklittlefs -c private/first-install-fs -b 4096 -p 256 -s 1769472 private/littlefs.bin
```

5. After explicit user authorization, chip verification and backup, a first-install
   idf.py flash writes bootloader/partition table/app using the build's flash arguments.
   Install the separately generated filesystem image at **0x250000**, size 1728 KiB,
   using esptool with target esp32 and the individually confirmed COM port.
   No automatic flash script or merged firmware binary is distributed.

The filesystem partition retains the legacy label spiffs but contains **LittleFS**,
not SPIFFS. Firmware will **not auto-format** a blank/corrupt/incompatible filesystem.
Mount failures stop startup and retain the existing bytes. Do not erase to make an
upgrade boot. Deliberately creating a new filesystem is a first-install action.

## Upgrading an existing working device

- Confirm the actual partition table still has 1152 KiB app slots at 0x10000 and
  0x130000 and LittleFS at 0x250000. Do not assume an unrelated board matches.
- Back up NVS, otadata, both application slots and filesystem privately. Validate
  backups and retain the working build, sdkconfig, ESBL and recovery instructions.
- **Do not run full flash, erase_flash, or write a new LittleFS image as an upgrade.**
  They can alter boot metadata or overwrite user/blocklist/rollback data.
- Select the intended app slot only after reviewing the existing boot/OTA state.
  A deliberate app-only esptool write to that verified slot preserves NVS and
  filesystem; do not blindly use 0x10000 if the device runs ota_1.
- Existing current-slot settings are loaded without overwriting them. If the current
  directory is absent and the other slot has settings, it is reused **in place**:
  no renaming/migration or removal of the prior directory. Invalid/incomplete data
  stops startup rather than being silently reset.
- Reusing the prior directory preserves its files but subsequent lease/settings
  writes can update shared data there. This is **not an immutable rollback snapshot**.
  Full backup is still necessary. No native OTA update or automatic rollback
  orchestration is offered in v0.1.1.
- Changing build-time credentials does not override an existing settings.json.
  Do not erase storage to change credentials. Plan a reviewed local/offline data
  edit and backup; there is deliberately no HTTP configuration endpoint.

## Separate on-device acceptance gate (pending)

With a separately authorized flash, verify an external upstream in serial output,
LAN IP and /status.json. Test direct blocked A/AAAA/CNAME/HTTPS, allowed A/AAAA/HTTPS,
EDNS 1232 and 4096, malformed datagrams and concurrent equal-ID requests.
Then repeat the Power Hub path manually; never change router settings automatically.
Run a long heap soak with minimum free heap, largest free block, queue pressure,
Wi-Fi reconnects and query logging both off/on. Exercise upgrade and power-loss
recovery separately. This source release work has performed none of those steps.
