#include "dns/dns.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cerrno>
static_assert(sizeof(Header) == 12, "DNS header layout");
static uint16_t u16(const std::vector<uint8_t>& b, size_t p) {
    if (p > b.size() || b.size()-p < 2) throw std::out_of_range("Short DNS integer");
    return (static_cast<uint16_t>(b[p]) << 8) | b[p+1];
}
// RFC 1035 pointers refer to prior packet data; expansion is capped at 255 octets
// and 32 pointer hops. Rebuild local questions, never retain dangling pointers.
static size_t parse_name(const std::vector<uint8_t>& b, size_t p, std::string& text,
                         std::vector<uint8_t>* wire = nullptr) {
    size_t next = 0, expanded = 1, hops = 0;
    bool jumped = false;
    text.clear(); if (wire) wire->clear();
    for (;;) {
        if (p >= b.size()) throw std::out_of_range("Short DNS name");
        uint8_t n = b[p];
        if (!n) { if (wire) wire->push_back(0); return jumped ? next : p+1; }
        if ((n & 0xc0) == 0xc0) {
            uint16_t target = u16(b,p) & 0x3fff;
            if (target < 12 || target >= p || ++hops > 32) throw std::out_of_range("Invalid DNS pointer");
            if (!jumped) next = p+2;
            jumped = true; p = target; continue;
        }
        if ((n & 0xc0) || n > 63 || b.size()-p-1 < n || expanded+n+1 > 255)
            throw std::out_of_range("Invalid DNS label");
        expanded += n+1;
        if (!text.empty()) text.push_back('.');
        if (wire) { wire->push_back(n); wire->insert(wire->end(), b.begin()+p+1, b.begin()+p+1+n); }
        for (size_t i = p+1; i <= p+n; ++i) {
            unsigned char c = b[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_') text += c;
            else { const char* hex = "0123456789abcdef"; text += "\\x"; text += hex[c>>4]; text += hex[c&15]; }
        }
        p += n+1;
    }
}
std::vector<uint8_t> Question::serialize() {
    auto b = qname; b.push_back(qtype>>8); b.push_back(qtype);
    b.push_back(qclass>>8); b.push_back(qclass); return b;
}
std::vector<uint8_t> ResourceRecord::serialize() {
    auto b = name; b.push_back(type>>8); b.push_back(type); b.push_back(clss>>8); b.push_back(clss);
    b.push_back(ttl>>24); b.push_back(ttl>>16); b.push_back(ttl>>8); b.push_back(ttl);
    b.push_back(rdlength>>8); b.push_back(rdlength); b.insert(b.end(), rddata.begin(), rddata.end()); return b;
}
DNS::DNS(std::vector<uint8_t>* b, sockaddr_in source, socklen_t length): addr(source), addrlen(length) {
    if (!b || b->size() < 12 || b->size() > MAX_PACKET_SIZE) throw std::out_of_range("Invalid DNS packet size");
    recv_timestamp = esp_timer_get_time(); memcpy(&header, b->data(), 12);
    if (header.opcode || header.z) throw std::out_of_range("Unsupported DNS header");
    size_t questions = ntohs(header.qcount), p = 12;
    if (questions != 1 && !(header.qr && header.rcode && questions == 0)) throw std::out_of_range("Unsupported question count");
    question.qtype = 0; question.qclass = 0;
    if (questions) {
        p = parse_name(*b, p, domain_name, &question.qname);
        question.qtype = u16(*b,p); question.qclass = u16(*b,p+2); p += 4;
        if (!header.qr && question.qclass != 1) throw std::out_of_range("Only IN queries supported");
    }
    if (!header.qr && (header.ancount || header.nscount || header.tc || header.rcode || header.aa || header.ra))
        throw std::out_of_range("Invalid query header");
    udp_limit = 512;
    bool opt = false;
    size_t counts[] = {ntohs(header.ancount), ntohs(header.nscount), ntohs(header.arcount)};
    for (size_t section = 0; section < 3; ++section) {
        // A truncated response can end after complete records. An incomplete
        // RR is not forwarded: callers can send a safe TC question response.
        for (size_t r = 0; r < counts[section]; ++r) {
            std::string owner; size_t owner_start = p;
            p = parse_name(*b,p,owner);
            if (p > b->size() || b->size()-p < 10) throw std::out_of_range("Short resource record");
            uint16_t type = u16(*b,p), clss = u16(*b,p+2), len = u16(*b,p+8);
            size_t data = p+10;
            if (len > b->size()-data) throw std::out_of_range("Short resource data");
            if (type == 41) {
                if (section != 2 || opt || (*b)[owner_start] != 0 || !owner.empty())
                    throw std::out_of_range("Invalid OPT owner or duplicate");
                opt = true;
                has_edns = true;
                if (!header.qr && ((*b)[p+4] || (*b)[p+5])) throw std::out_of_range("Unsupported EDNS version");
                udp_limit = std::max<uint16_t>(512, std::min<uint16_t>(MAX_PACKET_SIZE, clss));
                for (size_t o = data; o < data+len;) {
                    if (data+len-o < 4) throw std::out_of_range("Short EDNS option");
                    size_t n = u16(*b,o+2); o += 4;
                    if (n > data+len-o) throw std::out_of_range("Invalid EDNS option");
                    o += n;
                }
            } else if (!header.qr && type == 250) throw std::out_of_range("TSIG unsupported");
            else if (type == CNAME || type == NS || type == 12) {
                std::string unused;
                if (parse_name(*b,data,unused) != data+len) throw std::out_of_range("Invalid name resource data");
            }
            p = data+len;
        }
    }
    if (p != b->size()) throw std::out_of_range("Trailing DNS data");
    raw_packet = *b;
}
std::string DNS::convert_qname_url() { return domain_name; }
void DNS::rewrite_id(uint16_t id) {
    header.id = id; memcpy(raw_packet.data(), &id, 2);
}
std::string DNS::question_key() const {
    std::string key(question.qname.begin(), question.qname.end());
    // Preserve label boundaries and binary octets; fold ASCII inside labels only.
    for (size_t p = 0; p < key.size() && key[p];) {
        size_t n = static_cast<unsigned char>(key[p++]);
        for (size_t i = 0; i < n; ++i,++p) if (key[p] >= 'A' && key[p] <= 'Z') key[p] += 'a'-'A';
    }
    key += static_cast<char>(question.qtype>>8); key += static_cast<char>(question.qtype);
    key += static_cast<char>(question.qclass>>8); key += static_cast<char>(question.qclass);
    return key;
}
static Header local_header(Header h) {
    h.qr = 1; h.aa = 1; h.tc = 0; h.ad = 0; h.z = 0; h.ra = 1; h.rcode = 0;
    h.qcount = htons(1); h.ancount = 0; h.nscount = 0; h.arcount = 0;
    return h;
}
esp_err_t DNS::add_answer(const char* ip) {
    if (question.qtype != A && question.qtype != AAAA) return ESP_ERR_INVALID_ARG;
    header = local_header(header); records.clear();
    ResourceRecord r = {}; r.name = {0xc0,0x0c}; r.type = question.qtype; r.clss = 1; r.ttl = 128;
    r.rdlength = question.qtype == A ? 4 : 16; r.rddata.resize(r.rdlength);
    if (inet_pton(question.qtype == A ? AF_INET : AF_INET6, ip, r.rddata.data()) != 1) return ESP_ERR_INVALID_ARG;
    records.push_back(std::move(r)); header.ancount = htons(1); return ESP_OK;
}
std::vector<uint8_t> DNS::local_response(bool truncated) const {
    Header h = truncated ? header : local_header(header);
    h.qr = 1; h.ad = 0; h.qcount = htons(1); h.ancount = 0; h.nscount = 0; h.arcount = 0;
    if (truncated) { h.tc = 1; h.aa = 0; }
    else h.ancount = htons(static_cast<uint16_t>(records.size()));
    auto ptr = reinterpret_cast<const uint8_t*>(&h);
    std::vector<uint8_t> b(ptr, ptr+12);
    Question q = question; auto qb = q.serialize(); b.insert(b.end(), qb.begin(), qb.end());
    if (!truncated) for (auto& r : records) {
        ResourceRecord copy = r; auto rb = copy.serialize(); b.insert(b.end(),rb.begin(),rb.end());
    }
    if (has_edns) {
        // EDNS is hop-by-hop. A local answer acknowledges EDNS0, but does not
        // copy client options or claim DNSSEC authentication (DO/AD stay clear).
        b[10] = 0; b[11] = 1;
        b.insert(b.end(), {0, 0, 41, static_cast<uint8_t>(udp_limit >> 8),
                          static_cast<uint8_t>(udp_limit), 0, 0, 0, 0, 0, 0});
    }
    return b;
}
std::vector<uint8_t> DNS::failure_response() const {
    Header h = local_header(header); h.aa = 0; h.rcode = 2;
    auto ptr = reinterpret_cast<const uint8_t*>(&h);
    std::vector<uint8_t> b(ptr, ptr+12);
    Question q = question; auto qb = q.serialize(); b.insert(b.end(), qb.begin(), qb.end());
    if (has_edns) {
        b[11] = 1;
        b.insert(b.end(), {0, 0, 41, static_cast<uint8_t>(udp_limit >> 8),
                          static_cast<uint8_t>(udp_limit), 0, 0, 0, 0, 0, 0});
    }
    return b;
}
std::vector<uint8_t> DNS::client_response(const DNS& query, uint16_t original_id) const {
    auto b = raw_packet.size() <= query.udp_limit ? raw_packet : query.local_response(true);
    if (b.size() < 12) throw std::out_of_range("Short response");
    if (raw_packet.size() > query.udp_limit) {
        // Retain upstream error/RD/RA/CD bits, discard AD and unsafe RRs.
        b[2] = static_cast<uint8_t>((raw_packet[2] | 0x82) & ~0x04);
        b[3] = static_cast<uint8_t>(raw_packet[3] & ~0x20);
    }
    memcpy(b.data(), &original_id, 2); return b;
}
esp_err_t send_dns_datagram(int sock, sockaddr_in addr, const std::vector<uint8_t>& b) {
    for (int retry = 0; retry < 4; ++retry) {
        int n = sendto(sock, reinterpret_cast<const char*>(b.data()), b.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (n == static_cast<int>(b.size())) return ESP_OK;
        if (errno != ENOMEM) return ESP_FAIL;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    return ESP_FAIL;
}
esp_err_t DNS::send(int sock, sockaddr_in addr) { return send_dns_datagram(sock,addr,local_response()); }
esp_err_t DNS::send_blocked(int sock, sockaddr_in addr) { records.clear(); return send_dns_datagram(sock,addr,local_response()); }
esp_err_t DNS::send_raw(int sock, sockaddr_in addr) { return send_dns_datagram(sock,addr,raw_packet); }

