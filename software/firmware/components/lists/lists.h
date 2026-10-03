#pragma once
#include "esp_system.h"
#include <string>
struct BlocklistStatus { bool valid; uint32_t records; uint32_t bytes; };
BlocklistStatus blocklist_status();
esp_err_t initialize_blocklists();
bool valid_url(const char* url);
std::string canonicalize_hostname(const char* hostname);
bool in_blacklist(const char* domain);
