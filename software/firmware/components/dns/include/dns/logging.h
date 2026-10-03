#pragma once
#include "esp_system.h"
#include <ctime>
#include <string>
#include <vector>
struct Log_Entry { time_t time; uint16_t type; uint32_t client; std::string domain; bool blocked; };
bool query_logging_enabled();
std::vector<Log_Entry> query_log_snapshot();
esp_err_t log_query(std::string domain, bool blocked, uint16_t type, uint32_t client);
