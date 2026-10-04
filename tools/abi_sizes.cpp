// Compile only: obtain target ABI sizes without flashing or running firmware.
#include "dns/dns.h"
#include "dns/server.h"
#include "dns/forwarder.h"
#include "dns/metrics.h"
#include "dns/logging.h"
#include "cJSON.h"
extern "C" {
extern const uint32_t esper_abi_sizes[] = {
    sizeof(DNS), sizeof(Client), sizeof(Log_Entry), sizeof(cJSON),
    sizeof(std::string), sizeof(std::vector<uint8_t>), sizeof(Header), sizeof(Question),
    sizeof(DnsForwarder), sizeof(DnsMetrics)
};
}
