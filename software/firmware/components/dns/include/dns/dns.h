#ifndef DNS_H
#define DNS_H

#include <esp_system.h>
#include "lwip/sockets.h"
#include <string>
#include <vector>


// EDNS-aware DNS messages commonly exceed the historical 512-byte UDP limit.
// Keep the receive buffer bounded for the classic ESP32 and forward the raw
// packet unchanged so OPT records and compression are not re-serialized.
#define MAX_PACKET_SIZE 4096

/**
  * @brief structs and enums used to parse DNS packets
  * 
  * Documentation:
  * https://tools.ietf.org/html/rfc1035
  * https://www.freesoft.org/CIE/RFC/1035/39.htm
  * https://www2.cs.duke.edu/courses/fall16/compsci356/DNS/DNS-primer.pdf
  * 
  */

enum QRFlag {
    QUERY,
    ANSWER
};

enum RecordTypes {
    A=1,
    NS=2,
    CNAME=5,
    AAAA=28,
    HTTPS=65,
};

typedef struct header{
    uint16_t id;        // identification number
 
    uint8_t rd :1;      // recursion desired
    uint8_t tc :1;      // truncated message
    uint8_t aa :1;      // authoritive answer
    uint8_t opcode :4;  // purpose of message
    uint8_t qr :1;      // query/response flag
 
    uint8_t rcode :4;   // response code
    uint8_t cd :1;      // checking disabled
    uint8_t ad :1;      // authenticated data
    uint8_t z :1;       // its z! reserved
    uint8_t ra :1;      // recursion available

    uint16_t qcount;    // number of entries in the question section
    uint16_t ancount;   // number of resource records in the answer section
    uint16_t nscount;   // number of name server resource records in the authority records section
    uint16_t arcount;   // number of resource records in the additional records section
} Header;

class Question {
    public:
        std::vector<uint8_t> qname;
        uint16_t qtype;
        uint16_t qclass;
        IRAM_ATTR std::vector<uint8_t> serialize();
};

class ResourceRecord {
    public:
        std::vector<uint8_t> name;
        uint16_t type;
        uint16_t clss;
        uint32_t ttl;
        uint16_t rdlength;
        std::vector<uint8_t> rddata;
        IRAM_ATTR std::vector<uint8_t> serialize();
};

class DNS {
    private:
        IRAM_ATTR esp_err_t unpack_name(std::vector<uint8_t>* buffer, int* index, std::vector<uint8_t>* name);
        IRAM_ATTR esp_err_t unpack_vector(std::vector<uint8_t>* buffer, int* index, size_t size, std::vector<uint8_t>* dest);
        template<typename T>
        IRAM_ATTR esp_err_t unpack(std::vector<uint8_t>* buffer, int* index, T* dest);
    public:
        struct sockaddr_in addr;
        socklen_t addrlen;
        int64_t recv_timestamp;

        Header header = {};
        Question question = {};
        uint16_t udp_limit = 512;
        bool has_edns = false;
        std::vector<ResourceRecord> records;
        std::vector<uint8_t> raw_packet;

        IRAM_ATTR DNS(std::vector<uint8_t>* buffer, sockaddr_in addr_, socklen_t addrlen_);
        IRAM_ATTR std::string convert_qname_url();
        IRAM_ATTR esp_err_t add_answer(const char* ip_str);
        IRAM_ATTR esp_err_t send(int socket, struct sockaddr_in addr);
        esp_err_t send_blocked(int socket, struct sockaddr_in addr);
        IRAM_ATTR esp_err_t send_raw(int socket, struct sockaddr_in addr);
        void rewrite_id(uint16_t wire_id);
        std::string question_key() const;
        std::vector<uint8_t> local_response(bool truncated = false) const;
        std::vector<uint8_t> failure_response() const;
        std::vector<uint8_t> client_response(const DNS& query, uint16_t original_id) const;

    private:
        std::string domain_name;
};

esp_err_t send_dns_datagram(int socket, sockaddr_in address, const std::vector<uint8_t>& bytes);

#endif
