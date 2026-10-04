#pragma once
#include "dns/server.h"
constexpr size_t MAX_PENDING_CLIENTS = 16;
constexpr size_t MAX_STORED_QUERY_SIZE = 2048;
constexpr int64_t CLIENT_TIMEOUT_US = 5000000;
// A narrow socket boundary; the same worker-owned state machine is host tested.
struct ForwardTransport {
    virtual esp_err_t upstream_udp(const std::vector<uint8_t>&, const sockaddr_in&) = 0;
    virtual esp_err_t client_udp(const std::vector<uint8_t>&, const sockaddr_in&) = 0;
    virtual bool upstream_tcp(const sockaddr_in&, const std::vector<uint8_t>&, std::vector<uint8_t>&) = 0;
    virtual ~ForwardTransport() = default;
};
class DnsForwarder {
    sockaddr_in upstream;
    ForwardTransport& transport;
    std::vector<Client> clients;
    void fail(const Client&) noexcept;
public:
    DnsForwarder(const sockaddr_in&, ForwardTransport&);
    // One DNS worker owns this object; no lock is held across network I/O.
    void expire(int64_t now);
    esp_err_t submit(DNS& query, uint16_t seed, int64_t now);
    esp_err_t answer(const DNS& response, int64_t now);
    size_t pending() const { return clients.size(); }
};
