#pragma once
#include <string>
#include "dns/logging.h"
enum class DashboardRoute { Missing, Home, Script, Style, Status, QueryLog };
DashboardRoute dashboard_route(const std::string& uri, bool get);
struct SafeStatus {
    bool connected, blocking, indexed, query_logging;
    unsigned free_heap, min_heap, records, blocklist_bytes;
    unsigned long long uptime_seconds;
    std::string lan_ip, upstream;
};
std::string status_json(const SafeStatus& status);
std::string query_entry_json(const Log_Entry& entry);
