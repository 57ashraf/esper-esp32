#pragma once
#include "dns/dns.h"
// Upstream TCP retry only, not a client-facing TCP/53 listener.
// A single absolute deadline covers connect, all partial I/O and framing.
bool dns_tcp_exchange(const sockaddr_in& upstream, const std::vector<uint8_t>& query,
                      std::vector<uint8_t>& response, int64_t timeout_us = 3000000);
