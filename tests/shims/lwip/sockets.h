#pragma once
#include "esp_system.h"
#include <vector>
#include <cstring>
using socklen_t = int;
extern std::vector<uint8_t> sent_packet;
inline int host_sendto(int, const void* b, size_t n, int, const sockaddr*, socklen_t) {
    auto* p = static_cast<const uint8_t*>(b); sent_packet.assign(p,p+n); return static_cast<int>(n);
}
#define sendto host_sendto
