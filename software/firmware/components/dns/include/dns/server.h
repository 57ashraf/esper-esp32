#pragma once
#include "dns/dns.h"
#include <vector>
#define DNS_PORT 53
struct Client {
    sockaddr_in src_address;
    uint16_t id, original_id;
    std::string key;
    std::vector<uint8_t> raw_query;
    int64_t response_latency;
};
// IDs are rewritten per outstanding request, not indexed by a client's ID.
// Production supplies esp_random(); the seam makes collision tests deterministic.
inline uint16_t unused_transaction_id(const std::vector<Client>& clients, uint16_t seed) {
    for (;;) {
        bool used = false;
        for (const auto& c : clients) if (c.id == seed) { used = true; break; }
        if (!used) return seed;
        ++seed;
    }
}
inline bool upstream_matches(const DNS& packet, const Client& client, const sockaddr_in& upstream) {
    return packet.header.qr && packet.header.id == client.id &&
        packet.addr.sin_addr.s_addr == upstream.sin_addr.s_addr && packet.addr.sin_port == upstream.sin_port &&
        ((!packet.header.qcount && packet.header.rcode) || packet.question_key() == client.key);
}
void start_dns();
