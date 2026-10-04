#include "dns/dns.h"
#include "dns/server.h"
#include "dns/forwarder.h"
#include "dns/tcp_transport.h"
#include "dns/metrics.h"
#include "dns/logging.h"
#include "error.h"
#include "events.h"
#include "settings.h"
#include "lists.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include <memory>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include "esp_log.h"
static const char* TAG = "DNS";
constexpr unsigned PACKET_QUEUE_SIZE = 8;
static int dns_srv_sock = -1, upstream_sock = -1;
static QueueHandle_t packet_queue;

struct SocketTransport final : ForwardTransport {
    esp_err_t upstream_udp(const std::vector<uint8_t>& b, const sockaddr_in& a) override {
        return send_dns_datagram(upstream_sock, a, b);
    }
    esp_err_t client_udp(const std::vector<uint8_t>& b, const sockaddr_in& a) override {
        return send_dns_datagram(dns_srv_sock, a, b);
    }
    bool upstream_tcp(const sockaddr_in& a, const std::vector<uint8_t>& q, std::vector<uint8_t>& r) override {
        return dns_tcp_exchange(a, q, r);
    }
};
static SocketTransport transport;
static sockaddr_in upstream_address;
static std::string device_url;
static std::unique_ptr<DnsForwarder> forwarder;
static void listening_t(void*) {
    // Startup barrier: a failed second task creation must not kill a listener
    // that already owns C++ heap objects (vTaskDelete does not unwind its stack).
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    std::vector<uint8_t> buffer;
    try { buffer.reserve(MAX_PACKET_SIZE+1); }
    catch (...) { set_bit(ERROR_BIT); vTaskDelete(nullptr); return; }
    for (;;) {
        fd_set fds; FD_ZERO(&fds); FD_SET(dns_srv_sock, &fds); FD_SET(upstream_sock, &fds);
        timeval timeout = {}; timeout.tv_sec = 1;
        int largest = dns_srv_sock > upstream_sock ? dns_srv_sock : upstream_sock;
        if (select(largest+1, &fds, nullptr, nullptr, &timeout) <= 0) {
            vTaskDelay(pdMS_TO_TICKS(1)); continue;
        }
        int sock = FD_ISSET(upstream_sock, &fds) ? upstream_sock : dns_srv_sock;
        sockaddr_in addr = {}; socklen_t addrlen = sizeof(addr);
        buffer.resize(MAX_PACKET_SIZE+1);
        int size = recvfrom(sock, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&addr), &addrlen);
        dns_count(DnsMetric::Received);
        if (size < 12 || size > MAX_PACKET_SIZE || addrlen != sizeof(addr) || addr.sin_family != AF_INET) {
            dns_count(DnsMetric::Malformed); continue;
        }
        buffer.resize(size);
        try {
            std::unique_ptr<DNS> packet(new DNS(&buffer, addr, addrlen));
            if ((sock == upstream_sock) != static_cast<bool>(packet->header.qr) ||
                (!packet->header.qr && buffer.size() > MAX_STORED_QUERY_SIZE)) {
                dns_count(DnsMetric::Malformed); continue;
            }
            DNS* queued = packet.get();
            if (xQueueSend(packet_queue, &queued, 0) == pdTRUE) packet.release();
            else {
                dns_count(DnsMetric::QueueDrops);
                if (!packet->header.qr)
                    send_dns_datagram(dns_srv_sock, addr, packet->failure_response());
            }
        } catch (...) { dns_count(DnsMetric::Malformed); }
    }
}
static void dns_t(void*) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    try {
        for (;;) {
            DNS* received = nullptr;
            // Expiration runs when idle too, not only when another packet arrives.
            if (xQueueReceive(packet_queue, &received, pdMS_TO_TICKS(100)) != pdTRUE) {
                forwarder->expire(esp_timer_get_time()); continue;
            }
            std::unique_ptr<DNS> packet(received);
            try {
                if (packet->header.qr) { forwarder->answer(*packet, esp_timer_get_time()); continue; }
                auto domain = packet->convert_qname_url();
                auto type = packet->question.qtype;
                bool address = type == A || type == AAAA;
                bool blockable = address || type == CNAME || type == HTTPS;
                bool local = address && !device_url.empty() && canonicalize_hostname(domain.c_str()) == device_url;
                bool blocked = !local && blockable && setting::read_bool(setting::BLOCK) && in_blacklist(domain.c_str());
                if (local || blocked) {
                    packet->records.clear();
                    if (type == A) {
                        auto answer = local ? setting::read_str(setting::IP) : std::string("0.0.0.0");
                        if (packet->add_answer(answer.c_str()) != ESP_OK) throw std::runtime_error("Invalid local address");
                    } else if (type == AAAA && !local) {
                        if (packet->add_answer("::") != ESP_OK) throw std::runtime_error("Invalid local address");
                    }
                    packet->send(dns_srv_sock, packet->addr);
                    if (blocked) dns_count(DnsMetric::Blocked);
                    set_bit(BLOCKED_QUERY_BIT);
                } else forwarder->submit(*packet, static_cast<uint16_t>(esp_random()), esp_timer_get_time());
                log_query(domain, blocked, type, packet->addr.sin_addr.s_addr);
            } catch (...) {
                if (packet && !packet->header.qr) {
                    try { send_dns_datagram(dns_srv_sock, packet->addr, packet->failure_response()); } catch (...) {}
                }
                ESP_LOGW(TAG, "DNS request failed; no query contents logged");
            }
        }
    } catch (...) {
        set_bit(ERROR_BIT);
        ESP_LOGE(TAG, "DNS worker initialization failed");
        vTaskDelete(nullptr);
    }
}
void start_dns() {
    ESP_LOGI(TAG, "Initializing DNS");
    upstream_address = {}; upstream_address.sin_family = AF_INET; upstream_address.sin_port = htons(DNS_PORT);
    auto ip = setting::read_str(setting::DNS_SRV);
    if (!ip4addr_aton(ip.c_str(), reinterpret_cast<ip4_addr_t*>(&upstream_address.sin_addr.s_addr)))
        THROWE(ESP_ERR_INVALID_ARG, "Invalid upstream DNS");
    auto hostname = setting::read_str(setting::HOSTNAME);
    device_url = canonicalize_hostname(hostname.c_str());
    forwarder.reset(new DnsForwarder(upstream_address, transport));
    ESP_LOGI(TAG, "Upstream DNS: %s (separate ephemeral UDP source port)", ip.c_str());
    initialize_blocklists();
    packet_queue = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(DNS*));
    if (!packet_queue) { forwarder.reset(); THROWE(ESP_ERR_NO_MEM, "DNS queue unavailable"); }
    dns_srv_sock = socket(AF_INET, SOCK_DGRAM, 0);
    upstream_sock = socket(AF_INET, SOCK_DGRAM, 0); // ephemeral source port, separate from LAN UDP/53
    if (dns_srv_sock < 0 || upstream_sock < 0) {
        if (dns_srv_sock >= 0) close(dns_srv_sock);
        if (upstream_sock >= 0) close(upstream_sock);
        vQueueDelete(packet_queue);
        forwarder.reset();
        THROWE(ESP_FAIL, "DNS socket unavailable");
    }
    sockaddr_in address = {}; address.sin_family = AF_INET;
    address.sin_port = htons(DNS_PORT); address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(dns_srv_sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(dns_srv_sock); close(upstream_sock); vQueueDelete(packet_queue);
        forwarder.reset();
        THROWE(ESP_FAIL, "DNS bind failed");
    }
    TaskHandle_t listener = nullptr, worker = nullptr;
    if (xTaskCreatePinnedToCore(listening_t, "listening_task", 8000, nullptr, 9, &listener, tskNO_AFFINITY) != pdPASS ||
        xTaskCreatePinnedToCore(dns_t, "dns_task", 15000, nullptr, 9, &worker, tskNO_AFFINITY) != pdPASS) {
        if (listener) vTaskDelete(listener);
        if (worker) vTaskDelete(worker);
        close(dns_srv_sock); close(upstream_sock);
        // Drain any objects queued while the first task was starting.
        DNS* queued = nullptr;
        while (xQueueReceive(packet_queue, &queued, 0) == pdTRUE) delete queued;
        vQueueDelete(packet_queue);
        forwarder.reset();
        THROWE(ESP_ERR_NO_MEM, "DNS task unavailable");
    }
    xTaskNotifyGive(listener); xTaskNotifyGive(worker);
    if (setting::read_bool(setting::BLOCK)) set_bit(BLOCKING_BIT);
    else clear_bit(BLOCKING_BIT);
}
