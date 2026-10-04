#include "dns/forwarder.h"
#include "dns/metrics.h"
#include <cstring>
DnsForwarder::DnsForwarder(const sockaddr_in& address, ForwardTransport& io): upstream(address), transport(io) {
    clients.reserve(MAX_PENDING_CLIENTS);
}
void DnsForwarder::fail(const Client& client) noexcept {
    try {
        auto bytes = client.raw_query;
        DNS query(&bytes, client.src_address, sizeof(sockaddr_in));
        query.rewrite_id(client.original_id);
        if (transport.client_udp(query.failure_response(), client.src_address) != ESP_OK)
            dns_count(DnsMetric::SendFailures);
    } catch (...) { dns_count(DnsMetric::SendFailures); }
}
void DnsForwarder::expire(int64_t now) {
    for (auto client = clients.begin(); client != clients.end();) {
        if (now >= client->response_latency && now - client->response_latency >= CLIENT_TIMEOUT_US) {
            dns_count(DnsMetric::Timeouts);
            fail(*client);
            client = clients.erase(client);
        } else ++client;
    }
}
esp_err_t DnsForwarder::submit(DNS& query, uint16_t seed, int64_t now) {
    expire(now);
    if (query.header.qr || query.raw_packet.size() > MAX_STORED_QUERY_SIZE) return ESP_ERR_INVALID_ARG;
    if (clients.size() == MAX_PENDING_CLIENTS) {
        dns_count(DnsMetric::Overloaded);
        try { transport.client_udp(query.failure_response(), query.addr); } catch (...) {}
        return ESP_ERR_NO_MEM;
    }
    try {
        Client client = {};
        client.src_address = query.addr;
        client.original_id = query.header.id;
        client.id = unused_transaction_id(clients, seed);
        client.key = query.question_key();
        client.response_latency = now;
        client.raw_query = query.raw_packet;
        memcpy(client.raw_query.data(), &client.id, 2);
        clients.push_back(std::move(client));
    } catch (...) {
        try { transport.client_udp(query.failure_response(), query.addr); } catch (...) {}
        return ESP_ERR_NO_MEM;
    }
    // Publish state before sending; remove it on *every* send failure, including exceptions.
    esp_err_t result = ESP_FAIL;
    try { result = transport.upstream_udp(clients.back().raw_query, upstream); } catch (...) {}
    if (result != ESP_OK) {
        dns_count(DnsMetric::SendFailures);
        fail(clients.back()); clients.pop_back();
    } else dns_count(DnsMetric::Forwarded);
    return result;
}
esp_err_t DnsForwarder::answer(const DNS& response, int64_t now) {
    expire(now);
    for (auto client = clients.begin(); client != clients.end(); ++client) {
        if (!upstream_matches(response, *client, upstream)) continue;
        esp_err_t result = ESP_FAIL;
        try {
            DNS query(&client->raw_query, client->src_address, sizeof(sockaddr_in));
            auto bytes = response.client_response(query, client->original_id);
            if (response.header.tc) {
                dns_count(DnsMetric::TcpAttempts);
                bool complete_answer = false;
                try {
                    std::vector<uint8_t> complete_bytes;
                    if (transport.upstream_tcp(upstream, client->raw_query, complete_bytes)) {
                        DNS complete(&complete_bytes, upstream, sizeof(upstream));
                        if (!complete.header.tc && upstream_matches(complete, *client, upstream)) {
                            bytes = complete.client_response(query, client->original_id);
                            complete_answer = true;
                        }
                    }
                } catch (...) {}
                if (!complete_answer) dns_count(DnsMetric::TcpFailures);
            }
            result = transport.client_udp(bytes, client->src_address);
        } catch (...) { fail(*client); }
        if (result == ESP_OK) dns_count(DnsMetric::Answered);
        else dns_count(DnsMetric::SendFailures);
        clients.erase(client);
        return result;
    }
    dns_count(DnsMetric::Unmatched);
    return ESP_FAIL;
}
