#include "filesystem.h"
#include "error.h"
#include "settings.h"
#include <cerrno>
#include <cstring>
#include <utility>
#include <unistd.h>
#ifdef ESPER_HOST_TEST
#include <windows.h>
#include <io.h>
#endif
#ifndef ESPER_HOST_TEST
#include "esp_ota_ops.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#endif
static std::string curr_dir("/fs");
#ifdef ESPER_HOST_TEST
static bool fail_sync = false, fail_rename = false;
void fs::test_root(const std::string& root) { curr_dir = root; }
void fs::test_fail_sync(bool fail) { fail_sync = fail; }
void fs::test_fail_rename(bool fail) { fail_rename = fail; }
#endif
static std::string path_in_root(const std::string& path) {
    if (path.empty() || path[0] != '/' || path.find("..") != std::string::npos ||
        path.find('\\') != std::string::npos || path.find('\0') != std::string::npos)
        THROWE(FS_ERR_INIT, "Invalid data path");
    return curr_dir + path;
}
fs::file::file(std::string path, const char* mode): fpath(std::move(path)) {
    handle = fopen(fpath.c_str(), mode);
    if (!handle) THROWE(errno, "Cannot open data file");
}
fs::file::~file() noexcept { if (handle) fclose(handle); }
fs::file::file(file&& other) noexcept: handle(other.handle), fpath(std::move(other.fpath)) { other.handle = nullptr; }
fs::file& fs::file::operator=(file&& other) noexcept {
    if (this != &other) {
        if (handle) fclose(handle);
        handle = other.handle; other.handle = nullptr; fpath = std::move(other.fpath);
    }
    return *this;
}
size_t fs::file::read(void* buffer, size_t size, size_t count) {
    if (!handle) THROWE(FS_ERR_INIT, "Read on closed file");
    size_t n = fread(buffer, size, count, handle);
    if (ferror(handle)) THROWE(errno, "Data read failed");
    return n;
}
size_t fs::file::write(const void* buffer, size_t size, size_t count) {
    if (!handle || fwrite(buffer, size, count, handle) != count) THROWE(FS_ERR_INIT, "Data write failed");
    return count;
}
void fs::file::sync_close() {
    if (!handle) THROWE(FS_ERR_INIT, "Sync on closed file");
#ifdef ESPER_HOST_TEST
    if (fail_sync) THROWE(FS_ERR_INIT, "Injected sync fault");
#endif
    if (fflush(handle)) THROWE(errno, "Data flush failed");
#ifndef ESPER_HOST_TEST
    if (fsync(fileno(handle))) THROWE(errno, "Data sync failed");
#else
    if (_commit(_fileno(handle))) THROWE(errno, "Data sync failed");
#endif
    FILE* closing = handle; handle = nullptr;
    if (fclose(closing)) THROWE(errno, "Data close failed");
}
fs::file fs::open(std::string path, const char* mode) { return file(path_in_root(path), mode); }
bool fs::exists(std::string path) {
    struct stat s;
    if (::stat(path_in_root(path).c_str(), &s) == 0) return true;
    if (errno == ENOENT) return false;
    THROWE(errno, "Cannot inspect data file");
}
struct stat fs::stat(std::string path) {
    struct stat s;
    if (::stat(path_in_root(path).c_str(), &s)) THROWE(errno, "Cannot inspect data file");
    return s;
}
void fs::unlink(std::string path) {
    if (::unlink(path_in_root(path).c_str())) THROWE(errno, "Cannot remove data file");
}
void fs::rename(std::string before, std::string after) {
#ifdef ESPER_HOST_TEST
    if (fail_rename) THROWE(FS_ERR_INIT, "Injected rename fault");
    // Windows CRT rename does not replace an existing destination.
    if (!MoveFileExA(path_in_root(before).c_str(), path_in_root(after).c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        THROWE(FS_ERR_INIT, "Atomic replacement failed");
#else
    if (::rename(path_in_root(before).c_str(), path_in_root(after).c_str()))
        THROWE(errno, "Atomic replacement failed");
#endif
}
void fs::select_data_directory(const std::string& current, const std::string& previous) {
    curr_dir = current;
    if (exists("/settings.json")) return; // Corruption is reported, never replaced.
    if (exists("/blacklist.txt") || exists("/blocklist.bin") || exists("/settings.json.tmp"))
        THROWE(FS_ERR_INIT, "Incomplete current data directory; recover manually");
    curr_dir = previous;
    if (exists("/settings.json")) return; // Reuse in place, do not consume rollback files.
    if (exists("/blacklist.txt") || exists("/blocklist.bin") || exists("/settings.json.tmp"))
        THROWE(FS_ERR_INIT, "Incomplete previous data directory; recover manually");
    curr_dir = current;
}
#ifndef ESPER_HOST_TEST
void init_fs() {
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = "/fs"; conf.partition_label = "spiffs";
    conf.format_if_mount_failed = false; conf.dont_mount = false;
    TRY(esp_vfs_littlefs_register(&conf))
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* previous = esp_ota_get_next_update_partition(nullptr);
    if (!running || !previous) THROWE(FS_ERR_INIT, "Missing application partitions");
    const std::string current = "/fs/" + std::string(running->label);
    fs::select_data_directory(current, "/fs/" + std::string(previous->label));
    if (!fs::exists("/settings.json")) {
        if (::mkdir(curr_dir.c_str(), 0700) && errno != EEXIST) THROWE(errno, "Cannot create data directory");
        extern const unsigned char start[] asm("_binary_defaultsettings_json_start");
        extern const unsigned char end[] asm("_binary_defaultsettings_json_end");
        { fs::file f = fs::open("/settings.json.tmp", "wb"); f.write(start, 1, end-start); f.sync_close(); }
        fs::rename("/settings.json.tmp", "/settings.json");
        setting::load_settings();
        setting::write(setting::SSID, CONFIG_WIFI_SSID);
        setting::write(setting::PASSWORD, CONFIG_WIFI_PASSWORD);
        ESP_LOGI("FILE", "Initialized new settings; install a blocklist separately");
    } else setting::load_settings();
}
#endif

