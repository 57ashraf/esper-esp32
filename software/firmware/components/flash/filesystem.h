#pragma once
#include "esp_system.h"
#include <cstdio>
#include <sys/stat.h>
#include <string>
namespace fs {
class file {
public:
    FILE* handle = nullptr;
    std::string fpath;
    file(std::string path, const char* mode);
    ~file() noexcept;
    file(const file&) = delete;
    file& operator=(const file&) = delete;
    file(file&& other) noexcept;
    file& operator=(file&& other) noexcept;
    size_t read(void* buffer, size_t size, size_t count);
    size_t write(const void* buffer, size_t size, size_t count);
    void sync_close();
};
file open(std::string path, const char* mode);
bool exists(std::string path);
struct stat stat(std::string path);
void unlink(std::string path);
void rename(std::string before, std::string after);
// Select an existing data directory without moving or deleting rollback data.
void select_data_directory(const std::string& current, const std::string& previous);
#ifdef ESPER_HOST_TEST
void test_root(const std::string& root);
void test_fail_sync(bool fail);
void test_fail_rename(bool fail);
#endif
}
void init_fs();
