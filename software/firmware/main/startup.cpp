#include "error.h"
#include "events.h"
#include "filesystem.h"
#include "settings.h"
#include "ip.h"
#include "webserver.h"
#include "dns/server.h"
#include "nvs_flash.h"
#include "esp_log.h"
extern "C" void app_main() {
    // The closed Wi-Fi library may include the SSID in its own info output.
    // Our callbacks emit state/reason only, never credentials.
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_log_level_set("wifi_init", ESP_LOG_NONE);
    try {
        init_event_group();
        set_bit(INITIALIZING_BIT);
        // Never erase NVS automatically; recovery requires a deliberate user action.
        TRY(nvs_flash_init())
        nvs_handle nvs;
        TRY(nvs_open("storage", NVS_READWRITE, &nvs))
        nvs_close(nvs);
        set_bit(WIFI_ENABLED_BIT);
        init_fs();
        init_interfaces();
        start_interfaces();
        wait_for(WIFI_GOT_IP_BIT, portMAX_DELAY);
        start_dns();
        TRY(start_webserver())
        clear_bit(INITIALIZING_BIT);
        ESP_LOGI("BOOT", "Experimental v0.1.1 ready on trusted LAN; hardware validation pending");
    } catch (...) {
        set_bit(ERROR_BIT);
        ESP_LOGE("BOOT", "Initialization failed; data retained. Recover locally.");
    }
}
