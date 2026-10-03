#include "settings.h"
#include "error.h"
#include "events.h"
#include "filesystem.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <memory>
#include <vector>
#include <cstring>
using namespace setting;
static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
static cJSON* settings = nullptr;
struct Guard {
    Guard() { if (!lock || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) THROWE(ESP_ERR_NO_MEM, "Settings lock unavailable"); }
    ~Guard() { xSemaphoreGive(lock); }
};
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
static const char* get_key(Key k) {
    static const char* keys[] = {"ip","netmask","gateway","ssid","password","update_srv","url","dns_srv","version","blocking","update_available"};
    if (k < IP || k > UPDATE_AVAILABLE) THROWE(SETTING_ERR_INVALID_KEY, "Unknown settings key");
    return keys[k];
}
static void validate(cJSON* root) {
    if (!cJSON_IsObject(root)) THROWE(SETTING_ERR_PARSE, "Settings must be an object");
    unsigned fields = 0;
    for (cJSON* o = root->child; o; o = o->next) {
        if (++fields > 16 || !o->string || strlen(o->string) > 32 ||
            (cJSON_IsString(o) && (!o->valuestring || strlen(o->valuestring) > 253)))
            THROWE(SETTING_ERR_PARSE, "Settings schema limits exceeded");
    }
    for (int k = IP; k <= VERSION; ++k) {
        if (k == UPDATE_SRV) continue; // legacy key is optional and unused
        cJSON* o = cJSON_GetObjectItemCaseSensitive(root, get_key(static_cast<Key>(k)));
        if (!cJSON_IsString(o) || !o->valuestring) THROWE(SETTING_ERR_WRONG_TYPE, "Invalid settings field");
        size_t limit = k == SSID ? 32 : k == PASSWORD ? 64 : 253;
        if (strlen(o->valuestring) > limit) THROWE(SETTING_ERR_PARSE, "Settings field exceeds limit");
    }
    if (!cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root, "blocking"))) THROWE(SETTING_ERR_WRONG_TYPE, "Invalid blocking field");
    // Reject duplicate keys: reading must not depend on parser ordering.
    for (cJSON* a = root->child; a; a = a->next)
        for (cJSON* b = a->next; b; b = b->next)
            if (!strcmp(a->string, b->string)) THROWE(SETTING_ERR_PARSE, "Duplicate settings key");
}
static void save_locked(cJSON* value) {
    validate(value);
    std::unique_ptr<char, decltype(&cJSON_free)> text(cJSON_PrintUnformatted(value), cJSON_free);
    if (!text || strlen(text.get()) > 4096) THROWE(SETTING_ERR_PARSE, "Settings serialization failed or too large");
    { fs::file tmp = fs::open("/settings.json.tmp", "wb"); tmp.write(text.get(), 1, strlen(text.get())); tmp.sync_close(); }
    fs::rename("/settings.json.tmp", "/settings.json"); // Never unlink the valid destination.
}
void setting::load_settings() {
    Guard guard;
    auto s = fs::stat("/settings.json");
    if (s.st_size <= 0 || s.st_size > 4096) THROWE(SETTING_ERR_PARSE, "Invalid settings size");
    std::vector<char> data(static_cast<size_t>(s.st_size) + 1, 0);
    fs::file f = fs::open("/settings.json", "rb");
    if (f.read(data.data(), 1, s.st_size) != static_cast<size_t>(s.st_size)) THROWE(SETTING_ERR_PARSE, "Short settings read");
    if (memchr(data.data(), 0, s.st_size) || std::string(data.data()).find("\\u0000") != std::string::npos)
        THROWE(SETTING_ERR_PARSE, "NUL in settings");
    // The settings schema is flat. Reject nesting before cJSON recursion can
    // consume an ESP32 task stack, including nested values under unknown keys.
    bool quoted = false, escaped = false;
    int depth = 0; unsigned fields = 0;
    for (size_t i = 0; i < static_cast<size_t>(s.st_size); ++i) {
        char c = data[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == ':' && ++fields > 16) { THROWE(SETTING_ERR_PARSE, "Too many settings fields"); }
        else if (c == '[' || c == ']' || (c == '{' && ++depth > 1) || (c == '}' && --depth < 0))
            THROWE(SETTING_ERR_PARSE, "Nested settings unsupported");
    }
    const char* end = nullptr;
    Json candidate(cJSON_ParseWithOpts(data.data(), &end, 1), cJSON_Delete);
    if (!candidate) THROWE(SETTING_ERR_PARSE, "Invalid settings JSON"); // Never log the input/error pointer.
    validate(candidate.get());
    cJSON_Delete(settings); settings = candidate.release();
}
void setting::save_settings() { Guard guard; save_locked(settings); }
std::string setting::read_str(Key k) {
    Guard guard;
    cJSON* o = cJSON_GetObjectItemCaseSensitive(settings, get_key(k));
    if (!cJSON_IsString(o)) THROWE(SETTING_ERR_WRONG_TYPE, "Invalid string settings key");
    return std::string(o->valuestring);
}
bool setting::read_bool(Key k) {
    Guard guard;
    cJSON* o = cJSON_GetObjectItemCaseSensitive(settings, get_key(k));
    if (!cJSON_IsBool(o)) THROWE(SETTING_ERR_WRONG_TYPE, "Invalid boolean settings key");
    return cJSON_IsTrue(o);
}
static void replace_locked(Key k, cJSON* item) {
    Json replacement(item, cJSON_Delete);
    Json candidate(cJSON_Duplicate(settings, 1), cJSON_Delete);
    if (!candidate || !replacement) THROWE(ESP_ERR_NO_MEM, "Settings allocation failed");
    if (!cJSON_ReplaceItemInObjectCaseSensitive(candidate.get(), get_key(k), replacement.get()))
        THROWE(SETTING_ERR_INVALID_KEY, "Missing settings key");
    replacement.release();
    save_locked(candidate.get());
    cJSON_Delete(settings); settings = candidate.release();
}
void setting::write(Key k, const char* value) {
    if (!value) THROWE(SETTING_ERR_PARSE, "Null settings value");
    Guard guard; replace_locked(k, cJSON_CreateString(value)); // No value logging.
}
void setting::write(Key k, bool value) {
    Guard guard; replace_locked(k, cJSON_CreateBool(value));
    if (k == BLOCK) { if (value) set_bit(BLOCKING_BIT); else clear_bit(BLOCKING_BIT); }
}
