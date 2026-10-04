#include "dns/metrics.h"
#include <atomic>
static std::atomic<uint32_t> counters[static_cast<unsigned>(DnsMetric::Count)] = {};
void dns_count(DnsMetric metric) {
    auto i = static_cast<unsigned>(metric);
    if (i < static_cast<unsigned>(DnsMetric::Count)) counters[i].fetch_add(1, std::memory_order_relaxed);
}
DnsMetrics dns_metrics() {
    DnsMetrics result;
    for (unsigned i = 0; i < static_cast<unsigned>(DnsMetric::Count); ++i)
        result.values[i] = counters[i].load(std::memory_order_relaxed);
    return result;
}
