#pragma once
#include <cstdint>
// Aggregate counters only: no names, addresses, credentials or per-client data.
// Independent relaxed atomic loads make a safe, approximate live snapshot.
enum class DnsMetric : unsigned {
    Received, Malformed, QueueDrops, Blocked, Forwarded, Answered,
    Unmatched, Overloaded, SendFailures, Timeouts, TcpAttempts, TcpFailures, Count
};
struct DnsMetrics { uint32_t values[static_cast<unsigned>(DnsMetric::Count)] = {}; };
void dns_count(DnsMetric metric);
DnsMetrics dns_metrics();
