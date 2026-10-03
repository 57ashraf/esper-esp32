#include "get_handlers.h"
#include "safe_dashboard.h"
#include "settings.h"
#include "lists.h"
#include "events.h"
#include "esp_timer.h"
#include "esp_system.h"
#ifndef ESPER_HOST_TEST
#define ASSET(SYMBOL) extern const unsigned char SYMBOL##_start[] asm("_binary_" #SYMBOL "_start"); \
                     extern const unsigned char SYMBOL##_end[] asm("_binary_" #SYMBOL "_end")
ASSET(homepage_html); ASSET(app_scripts_js); ASSET(stylesheet_css);
#else
static const unsigned char homepage_html_start[] = "host asset";
static const unsigned char* homepage_html_end = homepage_html_start + sizeof(homepage_html_start)-1;
static const unsigned char app_scripts_js_start[] = "host script";
static const unsigned char* app_scripts_js_end = app_scripts_js_start + sizeof(app_scripts_js_start)-1;
static const unsigned char stylesheet_css_start[] = "host style";
static const unsigned char* stylesheet_css_end = stylesheet_css_start + sizeof(stylesheet_css_start)-1;
#endif
static esp_err_t asset(httpd_req_t* r, const unsigned char* begin, const unsigned char* end, const char* type) {
    httpd_resp_set_type(r, type);
    return httpd_resp_send(r, reinterpret_cast<const char*>(begin), end-begin);
}
esp_err_t get_handler(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; frame-ancestors 'none'; base-uri 'none'");
    try {
        switch (dashboard_route(req->uri, req->method == HTTP_GET)) {
        case DashboardRoute::Home: return asset(req, homepage_html_start, homepage_html_end, "text/html");
        case DashboardRoute::Script: return asset(req, app_scripts_js_start, app_scripts_js_end, "text/javascript");
        case DashboardRoute::Style: return asset(req, stylesheet_css_start, stylesheet_css_end, "text/css");
        case DashboardRoute::Status: {
            auto b = blocklist_status();
            SafeStatus s = {check_bit(WIFI_GOT_IP_BIT), setting::read_bool(setting::BLOCK), b.valid, query_logging_enabled(),
                esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), b.records, b.bytes,
                static_cast<unsigned long long>(esp_timer_get_time()/1000000),
                setting::read_str(setting::IP), setting::read_str(setting::DNS_SRV)};
            auto json = status_json(s);
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, json.data(), json.size());
        }
        case DashboardRoute::QueryLog: {
            auto snapshot = query_log_snapshot(); // bounded, locked copy, no lock during network I/O
            httpd_resp_set_type(req, "application/json");
            if (httpd_resp_sendstr_chunk(req, "[") != ESP_OK) return ESP_FAIL;
            bool first = true;
            for (auto& entry : snapshot) {
                auto json = query_entry_json(entry);
                if (!first && httpd_resp_sendstr_chunk(req, ",") != ESP_OK) return ESP_FAIL;
                if (httpd_resp_send_chunk(req, json.data(), json.size()) != ESP_OK) return ESP_FAIL;
                first = false;
            }
            if (httpd_resp_sendstr_chunk(req, "]") != ESP_OK) return ESP_FAIL;
            return httpd_resp_sendstr_chunk(req, nullptr);
        }
        default: return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        }
    } catch (...) { return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Status unavailable"); }
}
esp_err_t register_get_handlers(httpd_handle_t server) {
    httpd_uri_t get = {};
    get.uri = "*"; get.method = HTTP_GET; get.handler = get_handler;
    return httpd_register_uri_handler(server, &get);
}
