#include "ip.h"
#include "wifi.h"
#include "error.h"
#include "events.h"
#include "settings.h"
#include "lwip/inet.h"
#include "esp_log.h"
static esp_netif_t* wifi_sta_netif;
static void ip_event_handler(void*, esp_event_base_t, int32_t id, void* data) {
    if (id == IP_EVENT_STA_GOT_IP) {
        auto* e = static_cast<ip_event_got_ip_t*>(data);
        try {
            setting::write(setting::IP, inet_ntoa(e->ip_info.ip));
            setting::write(setting::NETMASK, inet_ntoa(e->ip_info.netmask));
            setting::write(setting::GATEWAY, inet_ntoa(e->ip_info.gw));
        } catch (...) { ESP_LOGW("IP", "Could not persist lease; prior settings retained"); }
        ESP_LOGI("IP", "LAN address: " IPSTR, IP2STR(&e->ip_info.ip));
        set_bit(WIFI_GOT_IP_BIT);
    } else if (id == IP_EVENT_STA_LOST_IP) clear_bit(WIFI_GOT_IP_BIT);
}
void init_interfaces() {
    TRY(esp_netif_init())
    TRY(esp_event_loop_create_default())
    TRY(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler, nullptr))
    wifi_sta_netif = init_wifi_sta_netif();
    set_bit(WIFI_INITIALIZED_BIT);
}
void start_interfaces() {
    // Preserve legacy explicit/saved static configuration; empty defaults use DHCP.
    esp_netif_ip_info_t info = {};
    std::string ip = setting::read_str(setting::IP);
    if (!ip.empty()) {
        std::string mask = setting::read_str(setting::NETMASK), gateway = setting::read_str(setting::GATEWAY);
        if (!inet_aton(ip.c_str(), &info.ip) || !inet_aton(mask.c_str(), &info.netmask) ||
            !inet_aton(gateway.c_str(), &info.gw)) THROWE(IP_ERR_INIT, "Invalid static IPv4 configuration");
        TRY(esp_netif_dhcpc_stop(wifi_sta_netif))
        TRY(esp_netif_set_ip_info(wifi_sta_netif, &info))
    }
    TRY(esp_wifi_start())
    TRY(esp_wifi_connect())
}

