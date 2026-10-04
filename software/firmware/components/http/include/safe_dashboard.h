#pragma once
#include <string>
#include "dns/logging.h"
#include "dns/metrics.h"
enum class DashboardRoute { Missing, Home, Script, Style, Status, QueryLog };
DashboardRoute dashboard_route(const std::string& uri, bool get);
bool dashboard_origin_allowed(std::string host, std::string origin, const std::string& fetch_site,
                              const std::string& lan_ip, std::string hostname);
struct SafeStatus {
    bool connected, blocking, indexed, query_logging;
    unsigned free_heap, min_heap, records, blocklist_bytes;
    unsigned long long uptime_seconds;
    std::string lan_ip, upstream;
    DnsMetrics dns;
};
std::string status_json(const SafeStatus& status);
std::string query_entry_json(const Log_Entry& entry);
