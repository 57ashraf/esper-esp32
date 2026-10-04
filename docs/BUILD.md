# Bootstrap and clean builds

## Pinned dependencies

- ESP-IDF **v4.4.7**, target **esp32**; Xtensa GCC **8.4.0 / esp-2021r2-patch5**.
  Release checkout commit: **38eeba213aa695aabfd6d89aa9f5078dbe5a94c3**.
- Legacy EOL SDK: no expectation of new security fixes. Migration is deferred.
- Python supported by the pinned SDK installer; use **Python 3.10** for a fresh setup.
  Local validation used the existing Python 3.12.14 environment; that is not a
  promise that all legacy dependencies support arbitrary new Python versions.
- Git, SDK-provided CMake/Ninja, a serial driver only if your USB adapter needs one.
- LittleFS wrapper/core are vendored with licenses; no new component download.
- Blocklist tooling needs Python standard library only.
- Native C++ tests use LLVM-MinGW 20260922 (Windows UCRT x86_64), downloaded separately
  with the checksum pinned in CI. No system-wide compiler installation is required.
- POSIX host tests use Clang; Linux CI runs the same suites under ASan/UBSan.
- mklittlefs **2.5.1-2** (disk format 2.0) is needed only for deliberate first-install
  filesystem-image creation. Do not substitute a format-2.1 tool.

Use a short, non-synchronized workspace path on Windows. Do not place credentials
or build files in the publication export.

## SDK bootstrap

Obtain [Espressif ESP-IDF v4.4.7](https://github.com/espressif/esp-idf/tree/v4.4.7)
as a separate dependency checkout, including its submodules. For example, from a
chosen dependency directory:

```text
git clone --branch v4.4.7 --recursive https://github.com/espressif/esp-idf.git esp-idf-4.4.7
```

Windows PowerShell: run the SDK's install.ps1 for esp32, then dot-source export.ps1
in the same terminal. Linux: run install.sh esp32 and source export.sh.
Follow [the versioned Espressif setup guide](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/get-started/index.html).
Do not install a different SDK globally and assume it is compatible.

The firmware CMake configuration rejects numeric SDK versions other than 4.4.7.
Also verify the dependency checkout is the exact release tag, not a modified SDK
with the same numeric version. Keep the SDK outside this publication tree.

## Placeholder-only clean build

From the release repository root after exporting the SDK:

Windows PowerShell:

```powershell
$releaseBuild = Join-Path $PWD 'private/build'
$releaseConfig = Join-Path $PWD 'private/sdkconfig'
idf.py -C software/firmware -B $releaseBuild -D "SDKCONFIG=$releaseConfig" build
idf.py -C software/firmware -B $releaseBuild size
python -B tools/report_build.py $releaseBuild
python -B tools/measure_abi.py $releaseBuild
```

Linux/Bash:

```bash
idf.py -C "$PWD/software/firmware" -B "$PWD/private/build" -D "SDKCONFIG=$PWD/private/sdkconfig" build
idf.py -C "$PWD/software/firmware" -B "$PWD/private/build" size
python -B tools/report_build.py "$PWD/private/build"
python -B tools/measure_abi.py "$PWD/private/build"
```

The default Wi-Fi strings are empty placeholders: this build compiles but is not
usable Wi-Fi firmware. CI overrides them with explicit CI_PLACEHOLDER values,
never secrets. The absolute private build/config paths avoid -C path ambiguity.

A clean verification uses a **new empty build directory and new sdkconfig**,
not an old working configuration. Do not delete unrelated builds/backups to clean.

## Private local credentials

Use menuconfig in your own visible terminal, with the same private build/config
paths as above:

```powershell
idf.py -C software/firmware -B $releaseBuild -D "SDKCONFIG=$releaseConfig" menuconfig
```

On Linux use the same absolute paths from the Bash example with menuconfig in
place of build.

Under Experimental Esper fork enter the SSID/password locally and save.
Rebuild using the same command. Do not paste them into chat, issue reports or CI,
and do not commit sdkconfig, private files or compiled outputs.
examples/sdkconfig.example and examples/settings.example.json contain blank values.

## Tests and scan

From the repository root:

```text
python -B -m unittest discover -s software/tools/blocklist -p test_blocklist.py -v
python -B -m unittest discover -s tests -p "test_*.py" -v
python -B tools/scan_publication.py
python -B tests/run_host_tests.py
```

For native tests put the checksum-verified portable compiler bin directory on the
current terminal PATH, or set CC to clang.exe and CXX to clang++.exe. Compiler DLLs
must also be discoverable on PATH. The test harness compiles into temporary
directories and creates synthetic ESBL fixtures there; it does not access COM ports.
See tests/README.md for exactly what is and is not exercised.

CI has a source scan, Python tests, actual C++ host tests and an SDK-pinned
placeholder build. It has no firmware artifact upload, publish, flash or deployment
step. v0.1.0's push/tag CI passed; v0.1.1 has an additional Linux sanitizer job.
Check the exact candidate commit's runs, linked in release notes, before relying
on any release. CI also verifies the SDK checkout SHA, not just its numeric version.
