#include "lists.h"
#if defined(ESPER_TEST_COLLISION_HASH) && !defined(ESPER_HOST_TEST)
#error "Test-only ESBL hash override is forbidden in firmware"
#endif
#include "error.h"
#include "filesystem.h"

#include <esp_system.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#ifdef CONFIG_LOCAL_LOG_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#endif
#include "esp_log.h"
static const char *TAG = "LIST";

// ESBL v1 is intentionally a small, append-free format.  It is generated on
// the host and installed as /blocklist.bin in LittleFS.  Hashes are an index,
// never the final decision: every equal-hash candidate is byte-verified.
static const uint8_t BLOCKLIST_MAGIC[4] = {'E', 'S', 'B', 'L'};
static const uint16_t BLOCKLIST_VERSION = 1;
static const uint16_t BLOCKLIST_HEADER_SIZE = 40;
static const uint16_t BLOCKLIST_RECORD_SIZE = 16;
static const uint16_t BLOCKLIST_WILDCARD_HEADER_SIZE = 4;

static const uint8_t RULE_SUFFIX = 0x01;
static const uint8_t RULE_WILDCARD_SUBDOMAIN = 0x02;
static const uint8_t RULE_GENERIC_WILDCARD = 0x04;

static const size_t MAX_DOMAIN_LENGTH = 253;
static const size_t MAX_LABEL_LENGTH = 63;
static const size_t MAX_LABEL_COUNT = 128;
static const size_t MAX_TEXT_LINE_LENGTH = MAX_DOMAIN_LENGTH + 4;

struct BlocklistIndex
{
    bool valid;
    uint32_t entry_count;
    uint32_t record_offset;
    uint32_t string_offset;
    uint32_t string_size;
    uint32_t wildcard_offset;
    uint32_t wildcard_size;
    uint32_t file_size;
    uint32_t wildcard_count;
};

struct BlocklistRecord
{
    uint64_t hash;
    uint32_t string_offset;
    uint16_t string_length;
    uint8_t flags;
};

struct LabelSpan
{
    const char *start;
    size_t length;
};

static BlocklistIndex blocklist_index = {};
static SemaphoreHandle_t blocklist_mutex = NULL;
static bool blocklist_initialized = false;

static uint16_t read_le16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(data[1]) << 8;
}

static uint32_t read_le32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           static_cast<uint32_t>(data[1]) << 8 |
           static_cast<uint32_t>(data[2]) << 16 |
           static_cast<uint32_t>(data[3]) << 24;
}

static uint64_t read_le64(const uint8_t *data)
{
    uint64_t value = 0;
    for( size_t i = 0; i < 8; ++i )
        value |= static_cast<uint64_t>(data[i]) << (8 * i);
    return value;
}

static uint64_t fnv1a64(const char *text, size_t length)
{
#ifdef ESPER_TEST_COLLISION_HASH
    return 42; // Host-test-only: exercise real equal-hash candidate byte verification.
#endif
    uint64_t hash = 0xcbf29ce484222325ULL;
    for( size_t i = 0; i < length; ++i )
    {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

static bool is_ascii_alnum(uint8_t value)
{
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z');
}

static bool canonicalize_name(const char *input, bool allow_wildcards, std::string &result)
{
    result.clear();
    if( input == NULL )
        return false;

    size_t length = strlen(input);
    size_t begin = 0;
    while( begin < length &&
           (input[begin] == ' ' || input[begin] == '\t' ||
            input[begin] == '\r' || input[begin] == '\n') )
    {
        ++begin;
    }
    while( length > begin &&
           (input[length - 1] == ' ' || input[length - 1] == '\t' ||
            input[length - 1] == '\r' || input[length - 1] == '\n') )
    {
        --length;
    }
    while( length > begin && input[length - 1] == '.' )
        --length;

    if( length == begin || length - begin > MAX_DOMAIN_LENGTH )
        return false;

    result.reserve(length - begin);
    size_t label_length = 0;
    for( size_t i = begin; i < length; ++i )
    {
        uint8_t value = static_cast<uint8_t>(input[i]);
        if( value == '.' )
        {
            if( label_length == 0 || label_length > MAX_LABEL_LENGTH )
                return false;
            result.push_back('.');
            label_length = 0;
            continue;
        }

        if( is_ascii_alnum(value) || value == '-' || value == '_' ||
            (allow_wildcards && (value == '*' || value == '?')) )
        {
            if( label_length >= MAX_LABEL_LENGTH )
                return false;
            if( value >= 'A' && value <= 'Z' )
                value = static_cast<uint8_t>(value - 'A' + 'a');
            result.push_back(static_cast<char>(value));
            ++label_length;
            continue;
        }
        return false;
    }

    return label_length > 0 && label_length <= MAX_LABEL_LENGTH &&
           result.size() <= MAX_DOMAIN_LENGTH;
}

std::string canonicalize_hostname(const char *hostname)
{
    std::string result;
    if( canonicalize_name(hostname, false, result) )
        return result;
    return std::string();
}

bool valid_url(const char* url)
{
    std::string canonical;
    return canonicalize_name(url, true, canonical);
}

static bool suffix_matches(const std::string &domain,
                           const std::string &base,
                           bool descendants_only)
{
    if( !descendants_only && domain == base )
        return true;
    if( domain.size() <= base.size() )
        return false;
    size_t start = domain.size() - base.size();
    return domain.compare(start, base.size(), base) == 0 &&
           domain[start - 1] == '.';
}

static bool wildcard_subdomain_base(const std::string &pattern, std::string &base)
{
    if( pattern.size() <= 2 || pattern.compare(0, 2, "*.") != 0 )
        return false;

    base = pattern.substr(2);
    if( base.find('*') != std::string::npos ||
        base.find('?') != std::string::npos ||
        base.find('.') == std::string::npos )
    {
        return false;
    }

    std::string canonical_base;
    if( !canonicalize_name(base.c_str(), false, canonical_base) )
        return false;
    base = canonical_base;
    return true;
}

static size_t split_labels(const char *text, LabelSpan *labels, size_t capacity)
{
    if( text == NULL || *text == '\0' )
        return 0;

    size_t count = 0;
    const char *start = text;
    for( const char *cursor = text; ; ++cursor )
    {
        if( *cursor != '.' && *cursor != '\0' )
            continue;
        if( count >= capacity || cursor == start )
            return 0;
        labels[count].start = start;
        labels[count].length = static_cast<size_t>(cursor - start);
        ++count;
        if( *cursor == '\0' )
            break;
        start = cursor + 1;
    }
    return count;
}

// Iterative glob matching keeps '*' and '?' inside one DNS label.  In
// particular, no wildcard can consume a dot or jump across a label boundary.
static bool match_label_glob(const char *pattern, size_t pattern_length,
                             const char *value, size_t value_length)
{
    size_t pattern_index = 0;
    size_t value_index = 0;
    size_t last_star = static_cast<size_t>(-1);
    size_t star_value = 0;

    while( value_index < value_length )
    {
        if( pattern_index < pattern_length &&
            (pattern[pattern_index] == '?' ||
             pattern[pattern_index] == value[value_index]) )
        {
            ++pattern_index;
            ++value_index;
        }
        else if( pattern_index < pattern_length &&
                 pattern[pattern_index] == '*' )
        {
            last_star = pattern_index++;
            star_value = value_index;
        }
        else if( last_star != static_cast<size_t>(-1) )
        {
            pattern_index = last_star + 1;
            value_index = ++star_value;
        }
        else
        {
            return false;
        }
    }

    while( pattern_index < pattern_length && pattern[pattern_index] == '*' )
        ++pattern_index;
    return pattern_index == pattern_length;
}

static bool wildcard_matches(const char *pattern, const char *domain)
{
    LabelSpan pattern_labels[MAX_LABEL_COUNT];
    LabelSpan domain_labels[MAX_LABEL_COUNT];
    size_t pattern_count = split_labels(pattern, pattern_labels, MAX_LABEL_COUNT);
    size_t domain_count = split_labels(domain, domain_labels, MAX_LABEL_COUNT);
    if( pattern_count == 0 || domain_count == 0 )
        return false;

    // A single-label legacy pattern such as advertising* is useful on any
    // label, but still cannot consume the dots around that label.
    if( pattern_count == 1 )
    {
        for( size_t i = 0; i < domain_count; ++i )
        {
            if( match_label_glob(pattern_labels[0].start, pattern_labels[0].length,
                                domain_labels[i].start, domain_labels[i].length) )
            {
                return true;
            }
        }
        return false;
    }

    // Multi-label wildcard rules match a contiguous label sequence.  This
    // gives rules such as ad.* and *.ad.* their useful legacy meaning without
    // making '*' a cross-label wildcard.
    if( pattern_count > domain_count )
        return false;
    for( size_t start = 0; start + pattern_count <= domain_count; ++start )
    {
        bool matched = true;
        for( size_t offset = 0; offset < pattern_count; ++offset )
        {
            if( !match_label_glob(pattern_labels[offset].start,
                                  pattern_labels[offset].length,
                                  domain_labels[start + offset].start,
                                  domain_labels[start + offset].length) )
            {
                matched = false;
                break;
            }
        }
        if( matched )
            return true;
    }
    return false;
}

static bool read_file_at(FILE *handle, uint32_t offset, void *buffer, size_t length)
{
    if( fseek(handle, static_cast<long>(offset), SEEK_SET) != 0 )
        return false;
    return fread(buffer, 1, length, handle) == length;
}

static bool read_record(FILE *handle, const BlocklistIndex &index,
                        uint32_t record_number, BlocklistRecord &record)
{
    uint64_t offset = static_cast<uint64_t>(index.record_offset) +
                      static_cast<uint64_t>(record_number) * BLOCKLIST_RECORD_SIZE;
    if( offset > 0x7fffffffULL )
        return false;

    uint8_t raw[BLOCKLIST_RECORD_SIZE];
    if( !read_file_at(handle, static_cast<uint32_t>(offset), raw, sizeof(raw)) )
        return false;

    record.hash = read_le64(raw);
    record.string_offset = read_le32(raw + 8);
    record.string_length = read_le16(raw + 12);
    record.flags = raw[14];
    return record.string_offset <= index.string_size &&
           record.string_length <= index.string_size - record.string_offset &&
           raw[15] == 0 && record.string_length > 0 && record.string_length <= MAX_DOMAIN_LENGTH;
}

static bool load_index_header(FILE *handle, BlocklistIndex &index)
{
    uint8_t raw[BLOCKLIST_HEADER_SIZE];
    if( !read_file_at(handle, 0, raw, sizeof(raw)) )
        return false;
    if( memcmp(raw, BLOCKLIST_MAGIC, sizeof(BLOCKLIST_MAGIC)) != 0 ||
        read_le16(raw + 4) != BLOCKLIST_VERSION ||
        read_le16(raw + 6) != BLOCKLIST_HEADER_SIZE )
    {
        return false;
    }

    index.valid = false;
    index.entry_count = read_le32(raw + 8);
    index.record_offset = read_le32(raw + 12);
    index.string_offset = read_le32(raw + 16);
    index.string_size = read_le32(raw + 20);
    index.wildcard_offset = read_le32(raw + 24);
    index.wildcard_size = read_le32(raw + 28);
    index.file_size = read_le32(raw + 32);
    index.wildcard_count = read_le32(raw + 36);

    uint64_t record_end = static_cast<uint64_t>(index.record_offset) +
                          static_cast<uint64_t>(index.entry_count) * BLOCKLIST_RECORD_SIZE;
    uint64_t string_end = static_cast<uint64_t>(index.string_offset) + index.string_size;
    uint64_t wildcard_end = static_cast<uint64_t>(index.wildcard_offset) + index.wildcard_size;
    if( index.record_offset != BLOCKLIST_HEADER_SIZE ||
        record_end != index.string_offset ||
        string_end != index.wildcard_offset ||
        wildcard_end != index.file_size ||
        index.file_size < BLOCKLIST_HEADER_SIZE || index.file_size > 1728U * 1024U ||
        index.wildcard_count > index.wildcard_size / BLOCKLIST_WILDCARD_HEADER_SIZE )
    {
        return false;
    }

    if( fseek(handle, 0, SEEK_END) != 0 )
        return false;
    long actual_size = ftell(handle);
    if( actual_size < 0 || static_cast<uint64_t>(actual_size) != index.file_size )
        return false;

    // Validate the ordering needed by binary search and each record's string
    // bounds without loading the table into RAM.
    uint64_t previous_hash = 0;
    bool have_previous = false;
    for( uint32_t i = 0; i < index.entry_count; ++i )
    {
        if ((i & 127U) == 0) vTaskDelay(pdMS_TO_TICKS(1));
        BlocklistRecord record = {};
        if( !read_record(handle, index, i, record) ||
            !(record.flags & (RULE_SUFFIX | RULE_WILDCARD_SUBDOMAIN)) ||
            (record.flags & ~(RULE_SUFFIX | RULE_WILDCARD_SUBDOMAIN)) )
        {
            return false;
        }
        if( have_previous && record.hash < previous_hash )
            return false;
        char name[MAX_DOMAIN_LENGTH + 1] = {};
        std::string canonical;
        if (!read_file_at(handle, index.string_offset + record.string_offset, name, record.string_length) ||
            strlen(name) != record.string_length || !canonicalize_name(name, false, canonical) ||
            canonical != name || fnv1a64(name, record.string_length) != record.hash)
            return false;
        previous_hash = record.hash;
        have_previous = true;
    }

    uint32_t cursor = index.wildcard_offset;
    uint32_t wildcard_end_u32 = index.wildcard_offset + index.wildcard_size;
    for( uint32_t i = 0; i < index.wildcard_count; ++i )
    {
        if( cursor > wildcard_end_u32 || wildcard_end_u32 - cursor < BLOCKLIST_WILDCARD_HEADER_SIZE )
            return false;
        uint8_t wildcard_header[BLOCKLIST_WILDCARD_HEADER_SIZE];
        if( !read_file_at(handle, cursor, wildcard_header, sizeof(wildcard_header)) ||
            wildcard_header[0] != RULE_GENERIC_WILDCARD || wildcard_header[1] != 0 )
        {
            return false;
        }
        uint16_t length = read_le16(wildcard_header + 2);
        cursor += BLOCKLIST_WILDCARD_HEADER_SIZE;
        if( length == 0 || length > MAX_DOMAIN_LENGTH || length > wildcard_end_u32 - cursor )
            return false;
        char pattern[MAX_DOMAIN_LENGTH + 1] = {};
        std::string canonical;
        if (!read_file_at(handle, cursor, pattern, length) || strlen(pattern) != length ||
            !canonicalize_name(pattern, true, canonical) || canonical != pattern ||
            canonical.find_first_of("*?") == std::string::npos)
            return false;
        cursor += length;
    }
    if( cursor != wildcard_end_u32 )
        return false;

    index.valid = true;
    return true;
}

static bool ensure_blocklist_mutex()
{
    if( blocklist_mutex == NULL )
        blocklist_mutex = xSemaphoreCreateMutex();
    return blocklist_mutex != NULL;
}

esp_err_t initialize_blocklists()
{
    if( !ensure_blocklist_mutex() )
        return ESP_ERR_NO_MEM;
    if( xSemaphoreTake(blocklist_mutex, 100 / portTICK_PERIOD_MS) == pdFALSE )
        return ESP_ERR_TIMEOUT;

    blocklist_index = {};
    blocklist_initialized = true;
    try
    {
        if( !fs::exists("/blocklist.bin") )
        {
            ESP_LOGI(TAG, "No indexed blocklist; using /blacklist.txt overlay");
        }
        else
        {
            fs::file blocklist = fs::open("/blocklist.bin", "rb");
            BlocklistIndex candidate = {};
            if( load_index_header(blocklist.handle, candidate) )
            {
                blocklist_index = candidate;
                ESP_LOGI(TAG, "Indexed blocklist: %u suffix records, %u wildcard records, %u bytes",
                         candidate.entry_count, candidate.wildcard_count, candidate.file_size);
            }
            else
            {
                ESP_LOGW(TAG, "Ignoring invalid /blocklist.bin; using /blacklist.txt overlay");
            }
        }
    }
    catch( const Err &e )
    {
        ESP_LOGW(TAG, "Unable to inspect /blocklist.bin; using text overlay");
    }
    catch( ... )
    {
        ESP_LOGW(TAG, "Unexpected blocklist error; using text overlay");
    }

    xSemaphoreGive(blocklist_mutex);
    return ESP_OK;
}

static bool index_has_rule(FILE *handle, const BlocklistIndex &index,
                           const std::string &candidate, bool descendant)
{
    if( candidate.empty() || candidate.size() > MAX_DOMAIN_LENGTH )
        return false;

    uint64_t target = fnv1a64(candidate.c_str(), candidate.size());
    uint32_t low = 0;
    uint32_t high = index.entry_count;
    while( low < high )
    {
        uint32_t middle = low + (high - low) / 2;
        BlocklistRecord record = {};
        if( !read_record(handle, index, middle, record) )
            return false;
        if( record.hash < target )
            low = middle + 1;
        else
            high = middle;
    }

    char stored[MAX_DOMAIN_LENGTH + 1];
    while( low < index.entry_count )
    {
        BlocklistRecord record = {};
        if( !read_record(handle, index, low, record) || record.hash != target )
            break;
        bool flag_matches = (record.flags & RULE_SUFFIX) != 0 ||
                            (descendant && (record.flags & RULE_WILDCARD_SUBDOMAIN) != 0);
        if( flag_matches && record.string_length <= MAX_DOMAIN_LENGTH &&
            record.string_length == candidate.size() &&
            read_file_at(handle,
                         index.string_offset + record.string_offset,
                         stored, record.string_length) &&
            memcmp(stored, candidate.data(), record.string_length) == 0 )
        {
            return true;
        }
        ++low;
    }
    return false;
}

static bool index_matches_domain(FILE *handle, const BlocklistIndex &index,
                                 const std::string &domain)
{
    size_t start = 0;
    while( start < domain.size() )
    {
        std::string candidate = domain.substr(start);
        if( index_has_rule(handle, index, candidate, start != 0) )
            return true;
        size_t dot = domain.find('.', start);
        if( dot == std::string::npos )
            break;
        start = dot + 1;
    }

    uint32_t cursor = index.wildcard_offset;
    uint32_t end = index.wildcard_offset + index.wildcard_size;
    char pattern[MAX_DOMAIN_LENGTH + 1];
    for( uint32_t i = 0; i < index.wildcard_count; ++i )
    {
        if( cursor > end || end - cursor < BLOCKLIST_WILDCARD_HEADER_SIZE )
            return false;
        uint8_t header[BLOCKLIST_WILDCARD_HEADER_SIZE];
        if( !read_file_at(handle, cursor, header, sizeof(header)) ||
            header[0] != RULE_GENERIC_WILDCARD )
        {
            return false;
        }
        uint16_t length = read_le16(header + 2);
        cursor += BLOCKLIST_WILDCARD_HEADER_SIZE;
        if( length > MAX_DOMAIN_LENGTH || length > end - cursor )
            return false;
        if( !read_file_at(handle, cursor, pattern, length) )
            return false;
        pattern[length] = '\0';
        if( wildcard_matches(pattern, domain.c_str()) )
            return true;
        cursor += length;
    }
    return false;
}

static bool text_rule_matches(const char *line, const std::string &domain)
{
    if( line == NULL )
        return false;

    while( *line == ' ' || *line == '\t' )
        ++line;
    if( *line == '\0' || *line == '!' || *line == '#' || *line == '[' ||
        *line == '@' )
    {
        return false;
    }

    char pattern_buffer[MAX_DOMAIN_LENGTH + 1];
    size_t pattern_length = 0;
    if( line[0] == '|' && line[1] == '|' )
        line += 2;
    while( line[pattern_length] != '\0' &&
           line[pattern_length] != '^' && line[pattern_length] != '$' &&
           line[pattern_length] != '|' && line[pattern_length] != '/' &&
           line[pattern_length] != ' ' && line[pattern_length] != '\t' )
    {
        if( pattern_length >= MAX_DOMAIN_LENGTH )
            return false;
        pattern_buffer[pattern_length] = line[pattern_length];
        ++pattern_length;
    }
    pattern_buffer[pattern_length] = '\0';

    std::string pattern;
    if( !canonicalize_name(pattern_buffer, true, pattern) )
        return false;

    std::string base;
    if( wildcard_subdomain_base(pattern, base) )
        return suffix_matches(domain, base, true);
    if( pattern.find('*') == std::string::npos &&
        pattern.find('?') == std::string::npos )
    {
        return suffix_matches(domain, pattern, false);
    }
    return wildcard_matches(pattern.c_str(), domain.c_str());
}

static bool text_matches_domain(const std::string &domain)
{
    try
    {
        fs::file blacklist = fs::open("/blacklist.txt", "r");
        char line[MAX_TEXT_LINE_LENGTH + 1];
        while( fgets(line, sizeof(line), blacklist.handle) != NULL )
        {
            if (!strchr(line, '\n') && !feof(blacklist.handle)) {
                int c; while ((c = fgetc(blacklist.handle)) != EOF && c != '\n') {}
                continue; // Never reinterpret a fragment of an overlong rule.
            }
            line[strcspn(line, "\r\n")] = '\0';
            if( text_rule_matches(line, domain) )
                return true;
        }
    }
    catch( const Err &e )
    {
        return false;
    }
    catch( ... )
    {
        return false;
    }
    return false;
}

bool in_blacklist(const char* domain)
{
    std::string canonical;
    if( !canonicalize_name(domain, false, canonical) )
        return false;

    if( !blocklist_initialized )
        initialize_blocklists();
    if( !ensure_blocklist_mutex() ||
        xSemaphoreTake(blocklist_mutex, 100 / portTICK_PERIOD_MS) == pdFALSE )
    {
        ESP_LOGW(TAG, "Blacklist lookup mutex unavailable");
        return false;
    }

    bool blocked = false;
    try
    {
        if( blocklist_index.valid )
        {
            fs::file blocklist = fs::open("/blocklist.bin", "rb");
            blocked = index_matches_domain(blocklist.handle, blocklist_index, canonical);
        }
        if( !blocked )
            blocked = text_matches_domain(canonical);
    }
    catch( const Err &e )
    {
        blocked = text_matches_domain(canonical);
    }
    catch( ... )
    {
        blocked = text_matches_domain(canonical);
    }
    xSemaphoreGive(blocklist_mutex);

    return blocked;
}

BlocklistStatus blocklist_status() {
    if (!blocklist_mutex || xSemaphoreTake(blocklist_mutex, portMAX_DELAY) != pdTRUE) return {};
    BlocklistStatus s = {blocklist_index.valid, blocklist_index.entry_count, blocklist_index.file_size};
    xSemaphoreGive(blocklist_mutex); return s;
}
