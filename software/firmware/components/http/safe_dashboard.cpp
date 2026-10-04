#include "safe_dashboard.h"
#include "cJSON.h"
#include <memory>
#include <stdexcept>
static void fold(std::string& s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a'-'A';
}
bool dashboard_origin_allowed(std::string host, std::string origin, const std::string& site,
                              const std::string& ip, std::string hostname) {
    if (host.empty() || host.size() > 253 || origin.size() > 270 ||
        (!site.empty() && site != "none" && site != "same-origin")) return false;
    fold(host); fold(origin); fold(hostname);
    auto authority = host;
    auto colon = host.find(':');
    if (colon != std::string::npos) {
        if (host.substr(colon) != ":80") return false;
        host.resize(colon);
    }
    if (!host.empty() && host.back() == '.') host.pop_back();
    if (!hostname.empty() && hostname.back() == '.') hostname.pop_back();
    if (host.empty() || (host != ip && (hostname.empty() || host != hostname))) return false;
    return origin.empty() || origin == "http://" + authority;
}
DashboardRoute dashboard_route(const std::string& uri, bool get) {
    if (!get || uri.size() > 511) return DashboardRoute::Missing;
    if (uri == "/" || uri == "/index.html") return DashboardRoute::Home;
    if (uri == "/scripts.js") return DashboardRoute::Script;
    if (uri == "/stylesheet.css") return DashboardRoute::Style;
    if (uri == "/status.json") return DashboardRoute::Status;
    if (uri == "/querylog.json") return DashboardRoute::QueryLog;
    return DashboardRoute::Missing; // No decoding, traversal, filesystem, or query-string routes.
}
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
static std::string encode(cJSON* value) {
    if (!value) throw std::bad_alloc();
    std::unique_ptr<char, decltype(&cJSON_free)> text(cJSON_PrintUnformatted(value), cJSON_free);
    if (!text) throw std::bad_alloc();
    return std::string(text.get());
}
static void string_field(cJSON* j, const char* key, const std::string& v) {
    if (!cJSON_AddStringToObject(j, key, v.c_str())) throw std::bad_alloc();
}
static void number_field(cJSON* j, const char* key, double v) {
    if (!cJSON_AddNumberToObject(j, key, v)) throw std::bad_alloc();
}
static void bool_field(cJSON* j, const char* key, bool v) {
    if (!cJSON_AddBoolToObject(j, key, v)) throw std::bad_alloc();
}
std::string status_json(const SafeStatus& s) {
    Json j(cJSON_CreateObject(), cJSON_Delete);
    if (!j) throw std::bad_alloc();
    string_field(j.get(), "version", "0.1.1");
    string_field(j.get(), "sdk", "4.4.7 (EOL)");
    string_field(j.get(), "lan_ip", s.lan_ip.substr(0, 15));
    string_field(j.get(), "upstream", s.upstream.substr(0, 15));
    bool_field(j.get(), "connected", s.connected); bool_field(j.get(), "blocking", s.blocking);
    bool_field(j.get(), "indexed", s.indexed); bool_field(j.get(), "query_logging", s.query_logging);
    number_field(j.get(), "records", s.records); number_field(j.get(), "blocklist_bytes", s.blocklist_bytes);
    number_field(j.get(), "free_heap", s.free_heap); number_field(j.get(), "min_heap", s.min_heap);
    number_field(j.get(), "uptime_seconds", s.uptime_seconds);
    static const char* names[] = {"dns_received", "dns_malformed", "dns_queue_drops", "dns_blocked",
        "dns_forwarded", "dns_answered", "dns_unmatched", "dns_overloaded", "dns_send_failures",
        "dns_timeouts", "dns_tcp_attempts", "dns_tcp_failures"};
    static_assert(sizeof(names)/sizeof(*names) == static_cast<unsigned>(DnsMetric::Count), "Metric names");
    for (unsigned i = 0; i < static_cast<unsigned>(DnsMetric::Count); ++i)
        number_field(j.get(), names[i], s.dns.values[i]);
    return encode(j.get());
}
std::string query_entry_json(const Log_Entry& e) {
    Json j(cJSON_CreateObject(), cJSON_Delete);
    if (!j) throw std::bad_alloc();
    // DNS label octets need not be UTF-8. Render non-ASCII/control bytes as literal hex notation.
    std::string safe;
    const char* hex = "0123456789abcdef";
    for (unsigned char c : e.domain.substr(0, 253)) {
        if (c >= 32 && c < 127) safe += static_cast<char>(c);
        else { safe += "\\x"; safe += hex[c >> 4]; safe += hex[c & 15]; }
    }
    string_field(j.get(), "domain", safe);
    number_field(j.get(), "time", static_cast<double>(e.time));
    number_field(j.get(), "type", e.type);
    bool_field(j.get(), "blocked", e.blocked);
    // Client IP is deliberately omitted to reduce browsing metadata.
    return encode(j.get());
}
