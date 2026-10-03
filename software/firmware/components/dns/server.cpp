#include "dns/dns.h"
#include "dns/server.h"
#include "dns/logging.h"
#include "error.h"
#include "events.h"
#include "settings.h"
#include "lists.h"

#include "errno.h"
#include "stdio.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"

#include <stdexcept>
#include <fcntl.h>

#ifdef CONFIG_LOCAL_LOG_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif
#include "esp_log.h"
static const char *TAG = "DNS";

#define PACKET_QUEUE_SIZE 8
#define CLIENT_QUEUE_SIZE 16
#define MAX_STORED_QUERY_SIZE 2048
#define CLIENT_TIMEOUT_US (5LL * 1000LL * 1000LL)

static int dns_srv_sock;                                // Client-facing DNS socket bound to port 53
static int upstream_sock;                               // Unbound socket for upstream DNS (ephemeral source port)
static TaskHandle_t dns;                                // Handle for DNS task
static TaskHandle_t listening;                          // Handle for listening task
static QueueHandle_t packet_queue;                      // FreeRTOS queue of DNS query packets
static SemaphoreHandle_t client_mutex;
static std::vector<Client> client_queue;                // FIFO Array of clients waiting for DNS response


static bool tcp_ready(int socket, bool write, int64_t deadline)
{
    int64_t remaining = deadline - esp_timer_get_time();
    if (remaining <= 0) return false;
    timeval tv = {}; tv.tv_sec = remaining / 1000000; tv.tv_usec = remaining % 1000000;
    fd_set fds; FD_ZERO(&fds); FD_SET(socket, &fds);
    return select(socket+1, write ? nullptr : &fds, write ? &fds : nullptr, nullptr, &tv) > 0;
}
static bool send_tcp_exact(int socket, const uint8_t* buffer, size_t length, int64_t deadline)
{
    size_t offset = 0;
    while( offset < length )
    {
        if (!tcp_ready(socket, true, deadline)) return false;
        ssize_t sent = send(socket, buffer + offset, length - offset, 0);
        if( sent <= 0 )
            return false;
        offset += static_cast<size_t>(sent);
    }
    return true;
}

static bool recv_tcp_exact(int socket, uint8_t* buffer, size_t length, int64_t deadline)
{
    size_t offset = 0;
    while( offset < length )
    {
        if (!tcp_ready(socket, false, deadline)) return false;
        ssize_t received = recv(socket, buffer + offset, length - offset, 0);
        if( received <= 0 )
            return false;
        offset += static_cast<size_t>(received);
    }
    return true;
}

static bool query_upstream_tcp(const sockaddr_in& upstream_dns,
                               const std::vector<uint8_t>& query,
                               std::vector<uint8_t>& response)
{
    if( query.empty() || query.size() > UINT16_MAX )
        return false;

    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if( tcp_socket < 0 )
    {
        ESP_LOGW(TAG, "TCP fallback socket failed: %s", strerror(errno));
        return false;
    }

    timeval timeout = {};
    timeout.tv_sec = 3;
    setsockopt(tcp_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    int64_t deadline = esp_timer_get_time() + 3000000;
    bool success = false;
    do {
        if (fcntl(tcp_socket, F_SETFL, O_NONBLOCK) < 0) break;
        int connected = connect(tcp_socket, (const sockaddr*)&upstream_dns, sizeof(upstream_dns));
        int error = 0; socklen_t error_size = sizeof(error);
        if (connected < 0 && errno != EINPROGRESS) break;
        if (!tcp_ready(tcp_socket, true, deadline) ||
            getsockopt(tcp_socket, SOL_SOCKET, SO_ERROR, &error, &error_size) < 0 || error)
        {
            ESP_LOGW(TAG, "TCP fallback connect to %s failed: %s",
                     inet_ntoa(upstream_dns.sin_addr.s_addr), strerror(errno));
            break;
        }

        uint16_t query_length = htons(static_cast<uint16_t>(query.size()));
        if( !send_tcp_exact(tcp_socket, reinterpret_cast<uint8_t*>(&query_length), sizeof(query_length), deadline) ||
            !send_tcp_exact(tcp_socket, query.data(), query.size(), deadline) )
        {
            ESP_LOGW(TAG, "TCP fallback query send failed: %s", strerror(errno));
            break;
        }

        uint16_t response_length = 0;
        if( !recv_tcp_exact(tcp_socket, reinterpret_cast<uint8_t*>(&response_length), sizeof(response_length), deadline) )
        {
            ESP_LOGW(TAG, "TCP fallback length receive failed: %s", strerror(errno));
            break;
        }
        response_length = ntohs(response_length);
        if( response_length == 0 || response_length > MAX_PACKET_SIZE )
        {
            ESP_LOGW(TAG, "TCP fallback response length %u is invalid", response_length);
            break;
        }

        response.resize(response_length);
        if( !recv_tcp_exact(tcp_socket, response.data(), response.size(), deadline) )
        {
            ESP_LOGW(TAG, "TCP fallback response receive failed: %s", strerror(errno));
            response.clear();
            break;
        }
        success = true;
    } while(false);

    close(tcp_socket);
    return success;
}

static IRAM_ATTR void listening_t(void* parameters)
{
    ESP_LOGV(TAG, "Listening...");
    std::vector<uint8_t> buffer;
    while(1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(dns_srv_sock, &readfds);
        FD_SET(upstream_sock, &readfds);
        int max_sock = dns_srv_sock > upstream_sock ? dns_srv_sock : upstream_sock;

        timeval timeout = {};
        timeout.tv_sec = 1;
        int ready = select(max_sock + 1, &readfds, NULL, NULL, &timeout);
        if( ready < 0 )
        {
            ESP_LOGW(TAG, "DNS socket select failed: %s", strerror(errno));
            continue;
        }
        if( ready == 0 )
            continue;

        int receive_sock = -1;
        if( FD_ISSET(upstream_sock, &readfds) )
            receive_sock = upstream_sock;
        else if( FD_ISSET(dns_srv_sock, &readfds) )
            receive_sock = dns_srv_sock;
        if( receive_sock < 0 )
            continue;

        // recvfrom() uses addrlen as an in/out parameter.  Leaving it
        // uninitialized makes the source address of upstream replies
        // undefined on lwIP and can cause those replies to be discarded.
        sockaddr_in addr = {};
        socklen_t addrlen = sizeof(addr);

        buffer.resize(MAX_PACKET_SIZE + 1);
        ssize_t size = recvfrom(receive_sock, buffer.data(), MAX_PACKET_SIZE + 1, 0, (struct sockaddr *)&addr, &addrlen);
        if( size < 12 || size > MAX_PACKET_SIZE )
        {
            ESP_LOGW(TAG, "Error receiving DNS packet: %s", strerror(errno));
            continue;
        }
        buffer.resize(size);
        ESP_LOGV(TAG, "Received %d Byte Packet from %s", size, inet_ntoa(addr.sin_addr.s_addr));

        DNS* packet;
        try{
            packet = new DNS(&buffer, addr, addrlen);
            if ((receive_sock == upstream_sock) != static_cast<bool>(packet->header.qr) ||
                (!packet->header.qr && buffer.size() > MAX_STORED_QUERY_SIZE)) { delete packet; continue; }
        }catch(...){
            ESP_LOGW(TAG, "Received malformed or truncated DNS packet");
            continue;
        }
        
        if( xQueueSend(packet_queue, &packet, 0) == errQUEUE_FULL )
        {
            ESP_LOGE(TAG, "Queue Full, could not add packet");
            delete packet; packet = nullptr;
            continue;
        }
    }
}


static IRAM_ATTR void purge_expired_clients()
{
    int64_t now = esp_timer_get_time();
    for(auto client = client_queue.begin(); client != client_queue.end(); )
    {
        if( now - client->response_latency > CLIENT_TIMEOUT_US )
        {
            ESP_LOGW(TAG, "Expiring DNS client id %.4X for %s", ntohs(client->id), "expired");
            client = client_queue.erase(client);
        }
        else
        {
            ++client;
        }
    }
}

static esp_err_t forward_answer(DNS* packet, const sockaddr_in& upstream_dns)
{
    // Only dns_t touches the client table; keep the existing mutex for now.
    if (xSemaphoreTake(client_mutex, portMAX_DELAY) != pdTRUE) return ESP_FAIL;
    esp_err_t result = ESP_FAIL;
    try {
        purge_expired_clients();
        for (auto client = client_queue.begin(); client != client_queue.end(); ++client) {
            if (!upstream_matches(*packet, *client, upstream_dns)) continue;
            DNS query(&client->raw_query, client->src_address, sizeof(sockaddr_in));
            if (packet->header.tc) {
                std::vector<uint8_t> tcp_response;
                if (query_upstream_tcp(upstream_dns, client->raw_query, tcp_response)) {
                    try {
                        DNS complete(&tcp_response, upstream_dns, sizeof(upstream_dns));
                        if (upstream_matches(complete, *client, upstream_dns))
                            packet->raw_packet = complete.client_response(query, client->original_id);
                        else packet->raw_packet = packet->client_response(query, client->original_id);
                    } catch (...) { packet->raw_packet = packet->client_response(query, client->original_id); }
                } else packet->raw_packet = packet->client_response(query, client->original_id);
            } else packet->raw_packet = packet->client_response(query, client->original_id);
            result = packet->send_raw(dns_srv_sock, client->src_address);
            client_queue.erase(client); // A failed delivery is retried by the client, not stale state.
            break;
        }
    } catch (...) { ESP_LOGW(TAG, "DNS response handling failed"); }
    xSemaphoreGive(client_mutex);
    return result;
}
static esp_err_t add_client(DNS* packet)
{
    if (packet->raw_packet.size() > MAX_STORED_QUERY_SIZE) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(client_mutex, portMAX_DELAY) != pdTRUE) return ESP_FAIL;
    try {
        purge_expired_clients();
        if (client_queue.size() >= CLIENT_QUEUE_SIZE) {
            xSemaphoreGive(client_mutex); return ESP_ERR_NO_MEM; // Do not evict a live transaction.
        }
        Client client = {};
        client.src_address = packet->addr;
        client.original_id = packet->header.id;
        client.id = unused_transaction_id(client_queue, static_cast<uint16_t>(esp_random()));
        client.key = packet->question_key();
        client.response_latency = packet->recv_timestamp;
        client.raw_query = packet->raw_packet;
        memcpy(client.raw_query.data(), &client.id, 2);
        client_queue.push_back(std::move(client));
        packet->rewrite_id(client_queue.back().id);
    } catch (...) { xSemaphoreGive(client_mutex); return ESP_ERR_NO_MEM; }
    xSemaphoreGive(client_mutex); return ESP_OK;
}

static IRAM_ATTR void dns_t(void* parameters)
{
    struct sockaddr_in upstream_dns = {};
    upstream_dns.sin_family = PF_INET;
    upstream_dns.sin_port = htons(DNS_PORT);
    std::string ip = setting::read_str(setting::DNS_SRV);
    if( !ip4addr_aton(ip.c_str(), (ip4_addr_t *)&upstream_dns.sin_addr.s_addr) )
    {
        ESP_LOGE(TAG, "Invalid upstream DNS address: %s", ip.c_str());
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Upstream DNS: %s", inet_ntoa(upstream_dns.sin_addr.s_addr));

    std::string url = setting::read_str(setting::HOSTNAME);
    std::string device_url = canonicalize_hostname(url.c_str());

    DNS* packet = NULL;
    while(1) 
    {
        delete packet; packet = nullptr;

        BaseType_t xErr = xQueueReceive(packet_queue, &packet, portMAX_DELAY);
        if(xErr == pdFALSE)
        {
            ESP_LOGW(TAG, "Error receiving from queue");
            continue;
        }

        try {
        std::string domain = packet->convert_qname_url();

        if( packet->header.qr == ANSWER ) // Forward all answers
        {
            forward_answer(packet, upstream_dns);
        }
        else if( packet->header.qr == QUERY )
        {
            uint16_t qtype = packet->question.qtype;
            bool address_query = qtype == A || qtype == AAAA;
            bool blockable_query = address_query || qtype == CNAME || qtype == HTTPS;
            if( blockable_query )
            {
                vTaskDelay(0); // This yields to higher priority tasks, watchdog may get triggered without this
                if( address_query && !device_url.empty() &&
                    canonicalize_hostname(domain.c_str()) == device_url ) // Check qname matches current device url
                {
                    std::string ip_str = setting::read_str(setting::IP);
                    packet->records.clear();
                    packet->header.arcount = 0;
                    if (qtype == A) {
                        packet->add_answer(ip_str.c_str());
                        packet->send(dns_srv_sock, packet->addr);
                    } else packet->send_blocked(dns_srv_sock, packet->addr);
                    log_query(domain, false, qtype, packet->addr.sin_addr.s_addr);
                    set_bit(BLOCKED_QUERY_BIT);
                }
                else if( setting::read_bool(setting::BLOCK) && in_blacklist(domain.c_str()) ) // check if url is in blacklist
                {
                    packet->records.clear();
                    packet->header.arcount = 0;
                    if( qtype == A )
                        packet->add_answer("0.0.0.0");
                    else if( qtype == AAAA )
                        packet->add_answer("::");
                    if( address_query )
                        packet->send(dns_srv_sock, packet->addr);
                    else
                        packet->send_blocked(dns_srv_sock, packet->addr);
                    log_query(domain, true, qtype, packet->addr.sin_addr.s_addr);
                    set_bit(BLOCKED_QUERY_BIT);
                }
                else
                {
                    if( add_client(packet) == ESP_OK )
                    {
                        esp_err_t result = packet->send_raw(upstream_sock, upstream_dns);
                        ESP_LOGD(TAG, "Sent raw upstream query id %.4X type %u len %u AR %u to %s (result %d)",
                                 ntohs(packet->header.id), qtype, packet->raw_packet.size(),
                                 ntohs(packet->header.arcount),
                                 inet_ntoa(upstream_dns.sin_addr.s_addr), result);
                    }
                    log_query(domain, false, qtype, packet->addr.sin_addr.s_addr);
                }
            }
            else // Preserve raw forwarding for all other query types.
            {
                if( add_client(packet) == ESP_OK )
                {
                    esp_err_t result = packet->send_raw(upstream_sock, upstream_dns);
                    ESP_LOGD(TAG, "Sent raw upstream query id %.4X type %u len %u AR %u to %s (result %d)",
                             ntohs(packet->header.id), qtype, packet->raw_packet.size(),
                             ntohs(packet->header.arcount),
                             inet_ntoa(upstream_dns.sin_addr.s_addr), result);
                }
                log_query(domain, false, qtype, packet->addr.sin_addr.s_addr);
            }
        }

        } catch (...) { ESP_LOGW(TAG, "DNS request handling failed"); }
        int64_t end = esp_timer_get_time();
        ESP_LOGV(TAG, "Processing Time: %lld ms", (end-packet->recv_timestamp)/1000);
    }
}

void start_dns()
{
    ESP_LOGI(TAG, "Initializing DNS...");
    initialize_blocklists();
    client_queue.reserve(CLIENT_QUEUE_SIZE);
    client_mutex = xSemaphoreCreateMutex();
    packet_queue = xQueueCreate(PACKET_QUEUE_SIZE, sizeof(DNS*));
    if( packet_queue == NULL || client_mutex == NULL)
    {
        THROWE(ESP_ERR_NO_MEM, "Error Initializing FreeRTOS structures for dns server")
    }

    // initialize listening socket
    ESP_LOGV(TAG, "Initializing Socket");
	if( (dns_srv_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
        THROWE(errno, "Socket init failed %s", strerror(errno))
    }

    // Use a separate unbound socket for upstream traffic. Reusing the
    // client-facing UDP/53 socket makes the ESP32 send with source port 53,
    // which can be rejected or mishandled by a home NAT/firewall when the
    // upstream resolver is outside the LAN.
    if( (upstream_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
    {
        THROWE(errno, "Upstream socket init failed %s", strerror(errno))
    }
    ESP_LOGI(TAG, "Upstream DNS socket uses an ephemeral source port");

    // Create socket for listening on port 53, allow from any IP address
    struct sockaddr_in my_addr = {};
	my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(DNS_PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if( bind(dns_srv_sock, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0 )
    {
        THROWE(errno, "Socket bind failed %s", strerror(errno))
    }

    BaseType_t xErr = xTaskCreatePinnedToCore(listening_t, "listening_task", 8000, NULL, 9, &listening, tskNO_AFFINITY);
    xErr &= xTaskCreatePinnedToCore(dns_t, "dns_task", 15000, NULL, 9, &dns, tskNO_AFFINITY);
    if( xErr != pdPASS )
    {
        THROWE(DNS_ERR_INIT, "Failed to start dns tasks");
    }

    bool blocking = setting::read_bool(setting::BLOCK);
    blocking ? set_bit(BLOCKING_BIT):clear_bit(BLOCKING_BIT);
    ESP_LOGV(TAG, "Blocking %s", blocking ? "on":"off");
}
