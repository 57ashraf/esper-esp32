#include "dns/logging.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#ifdef CONFIG_ESPER_QUERY_LOG_ENABLED
static SemaphoreHandle_t log_mutex = xSemaphoreCreateMutex();
static std::vector<Log_Entry> entries;
#endif
bool query_logging_enabled() {
#ifdef CONFIG_ESPER_QUERY_LOG_ENABLED
    return true;
#else
    return false;
#endif
}
std::vector<Log_Entry> query_log_snapshot() {
#ifdef CONFIG_ESPER_QUERY_LOG_ENABLED
    if (!log_mutex || xSemaphoreTake(log_mutex, portMAX_DELAY) != pdTRUE) return {};
    try { auto copy = entries; xSemaphoreGive(log_mutex); return copy; }
    catch (...) { xSemaphoreGive(log_mutex); throw; }
#else
    return {};
#endif
}
esp_err_t log_query(std::string domain, bool blocked, uint16_t type, uint32_t client) {
#ifdef CONFIG_ESPER_QUERY_LOG_ENABLED
    if (domain.size() > 253 || !log_mutex || xSemaphoreTake(log_mutex, portMAX_DELAY) != pdTRUE) return ESP_FAIL;
    try {
        if (entries.capacity() < 100) entries.reserve(100);
        if (entries.size() == 100) entries.erase(entries.begin());
        entries.push_back({time(nullptr), type, client, std::move(domain), blocked});
    } catch (...) { xSemaphoreGive(log_mutex); return ESP_ERR_NO_MEM; }
    xSemaphoreGive(log_mutex);
#endif
    return ESP_OK;
}
