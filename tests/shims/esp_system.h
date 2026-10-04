#pragma once
#include <cstdint>
#include <cstddef>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#endif
#define IRAM_ATTR
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NO_MEM=257, ESP_ERR_INVALID_ARG=258, ESP_ERR_TIMEOUT=263;
inline uint32_t esp_random() { return 1234; }
inline unsigned esp_get_free_heap_size() { return 100000; }
inline unsigned esp_get_minimum_free_heap_size() { return 90000; }
