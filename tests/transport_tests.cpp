// Real loopback sockets exercise the firmware's upstream TCP retry and UDP send.
// No external DNS, ESP32, network settings or privileged port is touched.
#include "dns/tcp_transport.h"
#include "esp_timer.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
using NativeSocket = SOCKET;
static void close_socket(NativeSocket s) { closesocket(s); }
#else
using NativeSocket = int;
static void close_socket(NativeSocket s) { close(s); }
#endif
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error("Transport check failed: " #x); } while(0)
struct Owner { NativeSocket s; ~Owner() { close_socket(s); } };
struct Join { std::thread& t; ~Join() { if (t.joinable()) t.join(); } };
static bool read_all(NativeSocket s, uint8_t* p, size_t n) {
    while (n) { int got=recv(s,reinterpret_cast<char*>(p),n,0); if(got<=0)return false; p+=got; n-=got; }
    return true;
}
static bool write_all(NativeSocket s,const uint8_t* p,size_t n) {
    while(n) {
#ifdef MSG_NOSIGNAL
        constexpr int flags=MSG_NOSIGNAL;
#else
        constexpr int flags=0;
#endif
        int sent=send(s,reinterpret_cast<const char*>(p),n,flags);
        if(sent<=0)return false; p+=sent; n-=sent;
    }
    return true;
}
static NativeSocket bound(int type,sockaddr_in& address) {
    NativeSocket s=socket(AF_INET,type,0);
    if(s==static_cast<NativeSocket>(-1))throw std::runtime_error("Loopback socket unavailable");
    address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(s,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0) {close_socket(s);throw std::runtime_error("Loopback bind failed");}
    socklen_t n=sizeof(address);if(getsockname(s,reinterpret_cast<sockaddr*>(&address),&n)<0){close_socket(s);throw std::runtime_error("Loopback address failed");}
    return s;
}
enum class Mode { Whole, Fragmented, ShortPrefix, ShortBody, Zero, Eleven, TooLarge, Stall, Dribble };
static void exchange(Mode mode,size_t payload_size=19) {
    sockaddr_in address; Owner listener{bound(SOCK_STREAM,address)};
    CHECK(listen(listener.s,1)==0);
    std::vector<uint8_t> query(payload_size,0);query[0]=0x12;query[1]=0x34;
    std::atomic<bool> server_ok{false};
    std::thread peer([&] {
        Owner client{accept(listener.s,nullptr,nullptr)};
        if(client.s==static_cast<NativeSocket>(-1))return;
        uint8_t prefix[2];if(!read_all(client.s,prefix,2))return;
        size_t n=(static_cast<size_t>(prefix[0])<<8)|prefix[1];
        std::vector<uint8_t> received(n);if(n!=query.size() || !read_all(client.s,received.data(),n) || received!=query)return;
        server_ok=true;
        if(mode==Mode::Stall){std::this_thread::sleep_for(std::chrono::milliseconds(600));return;}
        size_t advertised=mode==Mode::Zero?0:mode==Mode::Eleven?11:mode==Mode::TooLarge?MAX_PACKET_SIZE+1:n;
        prefix[0]=advertised>>8;prefix[1]=advertised;
        if(mode==Mode::ShortPrefix){write_all(client.s,prefix,1);return;}
        if(mode==Mode::Fragmented || mode==Mode::Dribble) {
            if(!write_all(client.s,prefix,1))return;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if(!write_all(client.s,prefix+1,1))return;
            for (size_t p=0;p<received.size();++p) {
                if(!write_all(client.s,received.data()+p,1))return;
                std::this_thread::sleep_for(std::chrono::milliseconds(mode==Mode::Dribble?30:1));
            }
        } else {
            if(!write_all(client.s,prefix,2))return;
            if(mode==Mode::Zero || mode==Mode::Eleven || mode==Mode::TooLarge)return;
            write_all(client.s,received.data(),mode==Mode::ShortBody?n-1:n);
        }
    });
    Join join{peer};
    std::vector<uint8_t> reply={1,2,3};
    auto begin=esp_timer_get_time();
    bool ok=dns_tcp_exchange(address,query,reply,(mode==Mode::Stall || mode==Mode::Dribble)?100000:1000000);
    auto elapsed=esp_timer_get_time()-begin;
    peer.join();
    CHECK(server_ok);
    CHECK(ok==(mode==Mode::Whole || mode==Mode::Fragmented));
    CHECK(ok?reply==query:reply.empty());
    if(mode==Mode::Stall || mode==Mode::Dribble)CHECK(elapsed<500000); // total deadline, generous scheduler margin
}
int main() {
#ifdef _WIN32
    WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data)!=0)return 1;
#endif
    try {
        for (auto mode : {Mode::Whole,Mode::Fragmented,Mode::ShortPrefix,Mode::ShortBody,Mode::Zero,Mode::Eleven,Mode::TooLarge,Mode::Stall,Mode::Dribble}) exchange(mode);
        exchange(Mode::Whole,MAX_PACKET_SIZE);
        sockaddr_in unused; {Owner old{bound(SOCK_STREAM,unused)};} // no listener at that address
        std::vector<uint8_t> q(12), r={9};CHECK(!dns_tcp_exchange(unused,q,r,100000) && r.empty());
        CHECK(!dns_tcp_exchange(unused,{},r) && r.empty());
        CHECK(!dns_tcp_exchange(unused,std::vector<uint8_t>(MAX_PACKET_SIZE+1),r));
        CHECK(!dns_tcp_exchange(unused,q,r,0));CHECK(!dns_tcp_exchange(unused,q,r,3000001));
        sockaddr_in destination;Owner receiver{bound(SOCK_DGRAM,destination)};Owner sender{socket(AF_INET,SOCK_DGRAM,0)};
        CHECK(send_dns_datagram(static_cast<int>(sender.s),destination,q)==ESP_OK);
        uint8_t bytes[12];sockaddr_in origin={};socklen_t length=sizeof(origin);
        CHECK(recvfrom(receiver.s,reinterpret_cast<char*>(bytes),sizeof(bytes),0,reinterpret_cast<sockaddr*>(&origin),&length)==12);
        CHECK(origin.sin_port!=htons(53) && origin.sin_port!=0); // actual ephemeral source port
        CHECK(std::equal(q.begin(),q.end(),bytes));
        std::cout<<"PASS: "<<checks<<" real-loopback transport checks (no hardware validation)\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
#ifdef _WIN32
    WSACleanup();
#endif
}
