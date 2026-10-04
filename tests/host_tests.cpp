// Tests link actual firmware DNS, ESBL, settings, storage, logging and dashboard logic.
#include "dns/dns.h"
#include "dns/server.h"
#include "dns/forwarder.h"
#include "dns/logging.h"
#include "safe_dashboard.h"
#include "filesystem.h"
#include "settings.h"
#include "error.h"
#include "events.h"
#include "cJSON.h"
#include "webserver.h"
#include "esp_http_server.h"
#include "../software/firmware/components/littlefs/src/fd_guard.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <iostream>
#include <random>
#include <type_traits>
#include "../software/firmware/components/lists/lists.cpp" // test private index verifier/collision walk directly
std::vector<uint8_t> sent_packet;
std::vector<httpd_uri_t> registered_routes;
const int BLOCKING_BIT=1;
const int WIFI_GOT_IP_BIT=2;
static std::atomic<bool> blocking{false};
bool check_bit(int bit) { return bit==WIFI_GOT_IP_BIT || blocking; }
void set_bit(int) { blocking = true; }
void clear_bit(int) { blocking = false; }
Err::Err(std::string s, int n, const char* f, const char* fn, int l) throw(): msg(s), errcode(n), file(f),func(fn),line(l) {}
const char* Err::what() const throw() { return msg.c_str(); }
static std::atomic<int> checks{0};
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error(std::string("CHECK failed: ")+#x+" line "+std::to_string(__LINE__)); } while(0)
template<class F> static bool fails(F f) { try { f(); return false; } catch (...) { return true; } }
static sockaddr_in source(uint32_t ip=1, uint16_t port=53) {
    sockaddr_in s={}; s.sin_family=AF_INET; s.sin_addr.s_addr=ip; s.sin_port=htons(port); return s;
}
static std::vector<uint8_t> query(const std::string& name="example.com", unsigned type=1, unsigned edns=0) {
    std::vector<uint8_t> b={0x12,0x34,1,0,0,1,0,0,0,0,0,0};
    size_t p=0;
    while (p<name.size()) { size_t end=name.find('.',p); if(end==std::string::npos)end=name.size();
        b.push_back(end-p); b.insert(b.end(),name.begin()+p,name.begin()+end); p=end+1; }
    b.insert(b.end(),{0,static_cast<uint8_t>(type>>8),static_cast<uint8_t>(type),0,1});
    if(edns) { b[11]=1; b.insert(b.end(),{0,0,41,static_cast<uint8_t>(edns>>8),static_cast<uint8_t>(edns),0,0,0,0,0,0}); }
    return b;
}
static DNS parse(std::vector<uint8_t> b) { return DNS(&b,source(),sizeof(sockaddr_in)); }
static void dns_tests() {
    auto q=query(); auto d=parse(q); CHECK(d.convert_qname_url()=="example.com"); CHECK(d.question.qtype==1); CHECK(d.udp_limit==512);
    for(unsigned t: {1U,28U,65U,5U,16U,257U}) { auto b=query("ExAmPlE.com",t); auto x=parse(b); CHECK(x.question.qtype==t); CHECK(x.question_key()==parse(query("example.com",t)).question_key()); }
    for(unsigned t: {1U,28U}) { auto x=parse(query("ad.example",t)); CHECK(x.add_answer(t==1?"0.0.0.0":"::")==ESP_OK);
        CHECK(x.send(1,source())==ESP_OK); auto ans=parse(sent_packet); CHECK(ans.header.qr); CHECK(ntohs(ans.header.ancount)==1); CHECK(!ans.header.ad); CHECK(ans.question.qtype==t);
    CHECK(sent_packet.back()==0); CHECK(sent_packet.size()==query("ad.example",t).size()+12+(t==1?4:16)); }
    for(unsigned t:{5U,65U}) { auto x=parse(query("ad.example",t)); CHECK(x.send_blocked(1,source())==ESP_OK);
        auto ans=parse(sent_packet); CHECK(ans.header.qr && !ans.header.ancount && ans.question.qtype==t); }
    CHECK(parse(query("example.com",1,1232)).udp_limit==1232);
    CHECK(parse(query("example.com",1,65535)).udp_limit==4096);
    CHECK(parse(query("example.com",1,128)).udp_limit==512);
    for (unsigned type : {1U,28U,5U,65U}) {
        auto edns_query = parse(query("ads.example",type,1232));
        if (type==1 || type==28) CHECK(edns_query.add_answer(type==1?"0.0.0.0":"::")==ESP_OK);
        auto edns_answer = parse(edns_query.local_response());
        CHECK(edns_answer.has_edns && edns_answer.udp_limit==1232 && ntohs(edns_answer.header.arcount)==1);
        CHECK(!edns_answer.header.ad && !edns_answer.header.rcode);
        auto failure=parse(edns_query.failure_response());
        CHECK(failure.header.rcode==2 && !failure.header.ancount && !failure.header.aa && failure.header.ra);
        CHECK(failure.has_edns && failure.question_key()==edns_query.question_key());
    }
    for(size_t n=0;n<q.size();++n) { auto b=q; b.resize(n); CHECK(fails([&]{parse(b);})); }
    for(int offset:{2,3,4,5,6,7,8,9,10}) { auto b=q; b[offset]=0xff; CHECK(fails([&]{parse(b);})); }
    auto b=q; b[12]=0xc0; b[13]=12; CHECK(fails([&]{parse(b);})); // self-pointer
    b=q; b[12]=0xc0; b[13]=20; CHECK(fails([&]{parse(b);})); // forward-pointer
    b=q; b[12]=0x40; CHECK(fails([&]{parse(b);})); // extended label unsupported
    b=q; b.push_back(0); CHECK(fails([&]{parse(b);})); // trailing bytes
    b=query("example.com",1,1232); size_t opt=b.size()-11; b[opt+6]=1; CHECK(fails([&]{parse(b);})); // EDNS version
    b=query("example.com",1,1232); b.back()=3; b.insert(b.end(),{0,1,0}); CHECK(fails([&]{parse(b);})); // short option header
    b=query("example.com",1,1232); b.back()=4; b.insert(b.end(),{0,1,0,8}); CHECK(fails([&]{parse(b);})); // option overflow
    b=query("example.com",1,1232); b[11]=2; b.insert(b.end(),b.begin()+opt,b.begin()+opt+11); CHECK(fails([&]{parse(b);})); // duplicate OPT
    b=q; b[2]|=0x80; b[7]=1; b.insert(b.end(),{0xc0,0x0c,0,5,0,1,0,0,0,60,0,2,0xc0,0x0c});
    auto compressed=parse(b); CHECK(compressed.header.ancount==htons(1)); // compressed owner/CNAME
    b.back()=255; CHECK(fails([&]{parse(b);}));
    auto long_name = std::string(63,'a')+"."+std::string(63,'b')+"."+std::string(63,'c')+"."+std::string(62,'d');
    CHECK(fails([&]{parse(query(long_name));}));
    CHECK(parse(query(std::string(63,'a')+"."+std::string(63,'b')+"."+std::string(63,'c')+"."+std::string(61,'d'))).question.qname.size()==255);
    // A valid prior name at offset >255 exercises all 14 pointer bits.
    b=q; b[2]|=0x80; b[7]=2;
    b.insert(b.end(),{0xc0,0x0c,0,16,0,1,0,0,0,60,1,44}); b.resize(b.size()+300,0);
    size_t prior=b.size(); b.insert(b.end(),{3,'f','o','o',0,0,5,0,1,0,0,0,60,0,2,0xc0,0x0c});
    b[7]=3; b.insert(b.end(),{static_cast<uint8_t>(0xc0|(prior>>8)),static_cast<uint8_t>(prior),0,1,0,1,0,0,0,60,0,4,1,2,3,4});
    CHECK(parse(b).header.ancount==htons(3));
    auto big=query(); big[2]|=0x80; big[7]=1;
    big.insert(big.end(),{0xc0,0x0c,0,16,0,1,0,0,0,60,2,88}); big.resize(big.size()+600,0);
    auto response=parse(big), small=parse(query());
    auto truncated=response.client_response(small,htons(99)); auto tc=parse(truncated);
    CHECK(tc.header.tc && !tc.header.ancount && ntohs(tc.header.id)==99 && truncated.size()<=512);
    auto large=parse(query("example.com",1,1232)); CHECK(response.client_response(large,htons(99)).size()==big.size());
    std::vector<uint8_t> error={0,1,0x81,2,0,0,0,0,0,0,0,0}; CHECK(parse(error).header.rcode==2);
    // Transaction collision: same question + same client ID, different client ports.
    auto answer=parse(q); answer.header.qr=1; answer.addr=source(9);
    Client c={}; c.original_id=htons(0x1234); c.id=htons(7); c.key=answer.question_key();
    std::vector<Client> table={c}; CHECK(unused_transaction_id(table,c.id)!=c.id);
    answer.header.id=c.id; CHECK(upstream_matches(answer,c,source(9)));
    CHECK(!upstream_matches(answer,c,source(8))); CHECK(!upstream_matches(answer,c,source(9,54)));
    auto other=parse(query("other.example")); other.header.qr=1; other.header.id=c.id; other.addr=source(9); CHECK(!upstream_matches(other,c,source(9)));
    answer.question.qclass=2; CHECK(!upstream_matches(answer,c,source(9)));
    Client c2=c; c2.src_address=source(1,9999); c2.id=unused_transaction_id(table,c.id); table.push_back(c2);
    answer.question.qclass=1; CHECK(upstream_matches(answer,c,source(9))); CHECK(!upstream_matches(answer,c2,source(9)));
    CHECK(c.original_id==c2.original_id && c.id!=c2.id);
    for(unsigned type:{1U,28U,65U}) {
        auto original_query=parse(query("normal.example",type,1232));
        auto allowed=query("normal.example",type,1232); allowed[2]|=0x80; allowed[7]=1;
        // Insert an answer before the OPT section; preserve answer and OPT bytes.
        size_t at=allowed.size()-11; std::vector<uint8_t> rr={0xc0,0x0c,0,static_cast<uint8_t>(type),0,1,0,0,0,60,0,0};
        std::vector<uint8_t> data=type==1?std::vector<uint8_t>{93,184,215,14}:type==28?std::vector<uint8_t>(16,1):std::vector<uint8_t>{0,1,0};
        rr[11]=data.size(); rr.insert(rr.end(),data.begin(),data.end()); allowed.insert(allowed.begin()+at,rr.begin(),rr.end());
        auto parsed=parse(allowed); auto forwarded=parsed.client_response(original_query,htons(99));
        CHECK(std::equal(allowed.begin()+2,allowed.end(),forwarded.begin()+2));
        CHECK(ntohs(parse(forwarded).header.id)==99);
    }
    // Deterministic malformed-packet fuzz smoke: 20,000 bounded inputs.
    std::mt19937 rng(7); for(int i=0;i<20000;++i) {
        std::vector<uint8_t> random(rng()%600); for(auto& v:random)v=rng();
        try { parse(random); } catch(const std::out_of_range&) {}
    }
}
struct FakeTransport final : ForwardTransport {
    struct Delivery { std::vector<uint8_t> bytes; sockaddr_in address; };
    std::vector<Delivery> upstream_packets, client_packets;
    std::vector<uint8_t> tcp_reply;
    bool fail_send=false, throw_send=false, throw_client=false, tcp_ok=false;
    esp_err_t upstream_udp(const std::vector<uint8_t>& b,const sockaddr_in& a) override {
        if (throw_send) throw std::bad_alloc();
        upstream_packets.push_back({b,a}); return fail_send ? ESP_FAIL : ESP_OK;
    }
    esp_err_t client_udp(const std::vector<uint8_t>& b,const sockaddr_in& a) override {
        if (throw_client) throw std::bad_alloc();
        client_packets.push_back({b,a}); return ESP_OK;
    }
    bool upstream_tcp(const sockaddr_in&,const std::vector<uint8_t>&,std::vector<uint8_t>& b) override {
        b=tcp_reply; return tcp_ok;
    }
};
static DNS forwarded_answer(const FakeTransport& io,size_t i=0,bool truncated=false) {
    auto b=io.upstream_packets.at(i).bytes; b[2]|=0x80;
    if (truncated) b[2]|=2;
    return DNS(&b,io.upstream_packets.at(i).address,sizeof(sockaddr_in));
}
static void forwarding_tests() {
    auto before=dns_metrics();
    FakeTransport io; DnsForwarder f(source(9),io);
    auto a=parse(query("normal.example",1,1232)), b=parse(query("normal.example",1,1232));
    a.addr=source(1,12000); b.addr=source(2,13000);
    CHECK(f.submit(a,htons(7),10)==ESP_OK); CHECK(f.submit(b,htons(7),11)==ESP_OK);
    CHECK(a.header.id==htons(0x1234) && b.header.id==a.header.id); // original objects not mutated
    CHECK(io.upstream_packets.size()==2 && f.pending()==2);
    CHECK(parse(io.upstream_packets[0].bytes).header.id!=parse(io.upstream_packets[1].bytes).header.id);
    auto wrong=forwarded_answer(io); wrong.addr=source(8);
    CHECK(f.answer(wrong,12)==ESP_FAIL && f.pending()==2 && io.client_packets.empty());
    wrong=forwarded_answer(io); wrong.addr.sin_port=htons(54); CHECK(f.answer(wrong,12)==ESP_FAIL);
    wrong=forwarded_answer(io); wrong.question.qtype=AAAA; CHECK(f.answer(wrong,12)==ESP_FAIL);
    auto second=forwarded_answer(io,1); CHECK(f.answer(second,12)==ESP_OK && f.pending()==1);
    CHECK(io.client_packets.back().address.sin_port==htons(13000));
    CHECK(parse(io.client_packets.back().bytes).header.id==htons(0x1234));
    CHECK(f.answer(second,12)==ESP_FAIL && io.client_packets.size()==1); // duplicate rejected
    auto first=forwarded_answer(io); CHECK(f.answer(first,13)==ESP_OK && f.pending()==0);
    CHECK(io.client_packets.back().address.sin_port==htons(12000));
    for(bool throws:{false,true}) {
        FakeTransport bad; bad.fail_send=!throws; bad.throw_send=throws; DnsForwarder g(source(9),bad);
        auto q=parse(query("normal.example",28,1232));
        CHECK(g.submit(q,7,0)==ESP_FAIL && g.pending()==0);
        CHECK(bad.client_packets.size()==1 && parse(bad.client_packets[0].bytes).header.rcode==2);
        CHECK(parse(bad.client_packets[0].bytes).header.id==q.header.id);
    }
    FakeTransport full; DnsForwarder table(source(9),full);
    for(size_t i=0;i<MAX_PENDING_CLIENTS;++i) { auto q=parse(query()); q.addr=source(1,20000+i); CHECK(table.submit(q,0,100)==ESP_OK); }
    auto overload=parse(query()); CHECK(table.submit(overload,0,100)==ESP_ERR_NO_MEM);
    CHECK(table.pending()==16 && full.upstream_packets.size()==16);
    CHECK(parse(full.client_packets.back().bytes).header.rcode==2);
    table.expire(100+CLIENT_TIMEOUT_US-1); CHECK(table.pending()==16);
    table.expire(100+CLIENT_TIMEOUT_US); CHECK(table.pending()==0 && full.client_packets.size()==17);
    CHECK(table.answer(forwarded_answer(full),100+CLIENT_TIMEOUT_US)==ESP_FAIL); // late reply
    auto new_q=parse(query()); CHECK(table.submit(new_q,42,100+CLIENT_TIMEOUT_US+1)==ESP_OK); // capacity recovered
    full.throw_client=true; table.expire(100+2*CLIENT_TIMEOUT_US+1); CHECK(table.pending()==0);
    for(int mode=0;mode<4;++mode) {
        FakeTransport retry; DnsForwarder g(source(9),retry); auto q=parse(query("normal.example",65,1232));
        CHECK(g.submit(q,8,0)==ESP_OK); auto tc=forwarded_answer(retry,0,true);
        retry.tcp_ok=mode!=0; retry.tcp_reply=retry.upstream_packets[0].bytes; retry.tcp_reply[2]|=0x80;
        if(mode==1) retry.tcp_reply[0]^=1; // mismatched transaction
        if(mode==2) retry.tcp_reply.resize(13); // malformed full response
        CHECK(g.answer(tc,1)==ESP_OK && g.pending()==0);
        auto delivered=parse(retry.client_packets.back().bytes);
        CHECK(delivered.header.tc==(mode!=3)); CHECK(delivered.header.id==q.header.id);
    }
    FakeTransport zero; DnsForwarder g(source(9),zero); auto q=parse(query()); CHECK(g.submit(q,9,0)==ESP_OK);
    auto error=zero.upstream_packets[0].bytes; error.resize(12); error[2]|=0x80; error[3]=2; error[5]=0;
    DNS err(&error,source(9),sizeof(sockaddr_in)); CHECK(g.answer(err,1)==ESP_OK && g.pending()==0);
    auto after=dns_metrics();
    CHECK(after.values[static_cast<unsigned>(DnsMetric::Overloaded)]-before.values[static_cast<unsigned>(DnsMetric::Overloaded)]==1);
    CHECK(after.values[static_cast<unsigned>(DnsMetric::TcpAttempts)]-before.values[static_cast<unsigned>(DnsMetric::TcpAttempts)]==4);
    CHECK(after.values[static_cast<unsigned>(DnsMetric::TcpFailures)]-before.values[static_cast<unsigned>(DnsMetric::TcpFailures)]==3);
}
static std::string read(const std::filesystem::path& p) { std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
static void write(const std::filesystem::path& p,const std::string& text) { std::ofstream f(p,std::ios::binary); f<<text; }
static void storage_tests(const std::filesystem::path& root) {
    static_assert(!std::is_copy_constructible<fs::file>::value,"file must be move-only");
    std::string config=R"({"ip":"","netmask":"","gateway":"","ssid":"fixture-ssid","password":"fixture-password","url":"esper.local","dns_srv":"8.8.8.8","version":"0.1.0","blocking":true})";
    write(root/"settings.json",config); fs::test_root(root.string()); setting::load_settings();
    CHECK(setting::read_str(setting::SSID)=="fixture-ssid");
    fs::test_fail_sync(true); CHECK(fails([]{setting::write(setting::DNS_SRV,"1.1.1.1");})); fs::test_fail_sync(false);
    CHECK(read(root/"settings.json")==config); CHECK(setting::read_str(setting::DNS_SRV)=="8.8.8.8");
    fs::test_fail_rename(true); CHECK(fails([]{setting::write(setting::DNS_SRV,"1.1.1.1");})); fs::test_fail_rename(false);
    CHECK(read(root/"settings.json")==config); CHECK(setting::read_str(setting::DNS_SRV)=="8.8.8.8");
    setting::write(setting::DNS_SRV,"1.1.1.1"); CHECK(setting::read_str(setting::DNS_SRV)=="1.1.1.1");
    for(const std::string& bad: {std::string("{"),config+"garbage",std::string(4097,'x'),
        std::string("{\"blocking\":true}"), config.substr(0,config.size()-1)+",\"ssid\":\"duplicate\"}",
        config.substr(0,config.size()-1)+",\"extra\":{\"nested\":true}}"}) {
        write(root/"settings.json",bad); CHECK(fails([]{setting::load_settings();}));
        CHECK(setting::read_str(setting::SSID)=="fixture-ssid"); CHECK(read(root/"settings.json")==bad);
    }
    write(root/"settings.json",config); setting::load_settings();
    CHECK(fails([]{fs::open("/../settings.json","rb");}));
    CHECK(fails([]{fs::open("settings.json","rb");}));
    void* descriptors[] = {reinterpret_cast<void*>(1),nullptr};
    CHECK(ESPER_LFS_VALID_FD(descriptors,2,0));
    CHECK(!ESPER_LFS_VALID_FD(descriptors,2,-1));
    CHECK(!ESPER_LFS_VALID_FD(descriptors,2,1));
    CHECK(!ESPER_LFS_VALID_FD(descriptors,2,2)); // exact upper bound, previously out-of-bounds
    { auto a=fs::open("/settings.json","rb"); auto b=std::move(a); CHECK(a.handle==nullptr); CHECK(b.handle!=nullptr); }
    std::atomic<bool> good{true};
    std::thread a([&]{try {for(int i=0;i<100;++i)setting::write(setting::BLOCK, bool(i%2));}catch(...){good=false;}});
    std::thread b([&]{try {for(int i=0;i<1000;++i)CHECK(setting::read_str(setting::SSID)=="fixture-ssid");}catch(...){good=false;}});
    a.join();b.join();CHECK(good);
    auto old=root/"previous", current=root/"current"; std::filesystem::create_directories(old); std::filesystem::create_directories(current);
    write(old/"settings.json",config); write(old/"blocklist.bin","rollback-fixture");
    fs::select_data_directory(current.string(),old.string()); CHECK(fs::exists("/settings.json")); CHECK(read(old/"blocklist.bin")=="rollback-fixture");
    write(current/"settings.json","corrupt"); fs::select_data_directory(current.string(),old.string());
    CHECK(fails([]{setting::load_settings();})); CHECK(read(current/"settings.json")=="corrupt"); CHECK(read(old/"settings.json")==config);
    fs::test_root(root.string());
}
static void list_tests(const std::filesystem::path& root) {
    fs::test_root(root.string()); CHECK(initialize_blocklists()==ESP_OK); CHECK(blocklist_status().valid);
    CHECK(in_blacklist("ADS.Example.")); CHECK(in_blacklist("sub.ads.example")); CHECK(!in_blacklist("notads.example"));
    CHECK(!in_blacklist("example.org")); CHECK(in_blacklist("sub.wild.example")); CHECK(!in_blacklist("wild.example"));
    CHECK(in_blacklist("track7.foo")); CHECK(!in_blacklist("track77.foo"));
    CHECK(!in_blacklist("xexample")); CHECK(!in_blacklist("a..ads.example"));
    auto bytes=read(root/"blocklist.bin");
    for(size_t offset:{0U,4U,6U,8U,12U,16U,20U,24U,28U,32U,36U,40U,48U,52U,54U,55U}) {
        auto bad=bytes; bad[offset]^=0xff; write(root/"blocklist.bin",bad); initialize_blocklists(); CHECK(!blocklist_status().valid);
    }
    auto bad=bytes; bad.pop_back(); write(root/"blocklist.bin",bad); initialize_blocklists(); CHECK(!blocklist_status().valid);
    write(root/"blocklist.bin",bytes); initialize_blocklists(); CHECK(blocklist_status().valid);
    write(root/"blacklist.txt",std::string(300,'x')+"overlay.example\n");
    CHECK(!in_blacklist("overlay.example")); // no fragment-based overlay matches
    write(root/"blacklist.txt","AD.*\n"); CHECK(in_blacklist("ad.site.test")); CHECK(!in_blacklist("bad.site.test"));
    // Same-hash adversarial fixture is compiled in a separate test executable.
#ifdef ESPER_TEST_COLLISION_HASH
    CHECK(!in_blacklist("collision-safe.example"));
#endif
}
static void dashboard_tests() {
    CHECK(dashboard_origin_allowed("192.0.2.1","","","192.0.2.1","esper.local"));
    CHECK(dashboard_origin_allowed("ESPER.local:80","http://esper.local:80","same-origin","192.0.2.1","esper.local"));
    CHECK(dashboard_origin_allowed("esper.local.","","none","192.0.2.1","esper.local"));
    for (const char* host : {"attacker.example","esper.local:81","esper.local:80:80","","esper.local@attacker.example","esper.local.."})
        CHECK(!dashboard_origin_allowed(host,"","","192.0.2.1","esper.local"));
    CHECK(!dashboard_origin_allowed("esper.local","http://attacker.example","","192.0.2.1","esper.local"));
    CHECK(!dashboard_origin_allowed("esper.local","null","","192.0.2.1","esper.local"));
    CHECK(!dashboard_origin_allowed("esper.local","","cross-site","192.0.2.1","esper.local"));
    for(const char* uri:{"/settings.json","/blacklist.txt","/blocklist.bin","/settings","/restart","/update","/ota","/../settings.json","/%2e%2e/settings.json","/status.json?settings"}) {
        CHECK(dashboard_route(uri,true)==DashboardRoute::Missing);
        CHECK(dashboard_route(uri,false)==DashboardRoute::Missing);
    }
    CHECK(dashboard_route("/status.json",true)==DashboardRoute::Status);
    CHECK(dashboard_route("/",false)==DashboardRoute::Missing);
    SafeStatus s={true,true,true,true,1000,900,3,100,5,"192.0.2.1","8.8.8.8",{}};
    auto encoded=status_json(s); CHECK(encoded.find("ssid")==std::string::npos); CHECK(encoded.find("password")==std::string::npos);
    CHECK(encoded.find("update_srv")==std::string::npos); CHECK(encoded.find("fixture-")==std::string::npos);
    CHECK(start_webserver()==ESP_OK); CHECK(registered_routes.size()==1);
    CHECK(registered_routes[0].method==HTTP_GET);
    for(const char* uri:{"/settings.json","/blacklist.txt","/blocklist.bin","/restart","/update","/../settings.json","/%2e%2e/settings.json"}) {
        httpd_req_t req={uri}; CHECK(registered_routes[0].handler(&req)==ESP_OK);
        CHECK(req.status==404); CHECK(req.response.find("fixture-")==std::string::npos);
        for(int method:{HTTP_POST,HTTP_PUT,HTTP_DELETE}) {
            httpd_req_t mutation={uri,method}; CHECK(registered_routes[0].handler(&mutation)==ESP_OK);
            CHECK(mutation.status==404); // no state-changing dispatch path even if called directly
        }
    }
    CHECK(setting::read_str(setting::SSID)=="fixture-ssid"); CHECK(setting::read_str(setting::PASSWORD)=="fixture-password");
    httpd_req_t status={"/status.json"}; CHECK(registered_routes[0].handler(&status)==ESP_OK); CHECK(status.status==200);
    CHECK(status.response.find("fixture-")==std::string::npos); CHECK(status.response.find("password")==std::string::npos);
    CHECK(status.headers.count("Content-Security-Policy")==1);
    CHECK(status.response.find("dns_timeouts")!=std::string::npos);
    httpd_req_t navigation={"/"}; navigation.request_headers["Sec-Fetch-Site"]="cross-site";
    navigation.request_headers["Sec-Fetch-Mode"]="navigate";
    CHECK(registered_routes[0].handler(&navigation)==ESP_OK && navigation.status==200);
    httpd_req_t foreign_status={"/status.json"}; foreign_status.request_headers=navigation.request_headers;
    CHECK(registered_routes[0].handler(&foreign_status)==ESP_OK && foreign_status.status==403);
    for (const char* key : {"Host","Origin","Sec-Fetch-Site"}) {
        httpd_req_t malicious={"/status.json"}; malicious.request_headers[key]=std::string(key)=="Sec-Fetch-Site"?"cross-site":"attacker.example";
        CHECK(registered_routes[0].handler(&malicious)==ESP_OK && malicious.status==403);
        CHECK(malicious.response.find("fixture-")==std::string::npos);
    }
    httpd_req_t missing_host={"/status.json"}; missing_host.request_headers.clear();
    CHECK(registered_routes[0].handler(&missing_host)==ESP_OK && missing_host.status==403);
    httpd_req_t giant_header={"/status.json"}; giant_header.request_headers["Host"]=std::string(254,'x');
    CHECK(registered_routes[0].handler(&giant_header)==ESP_OK && giant_header.status==500);
    Log_Entry e={};e.domain="\"\\\n<script>bad</script>"; auto json=query_entry_json(e);
    cJSON* p=cJSON_Parse(json.c_str());CHECK(p!=nullptr);cJSON_Delete(p);
    for(int i=0;i<150;++i) CHECK(log_query("example.com",true,28,1)==ESP_OK);
#ifdef CONFIG_ESPER_QUERY_LOG_ENABLED
    CHECK(query_log_snapshot().size()==100);CHECK(query_log_snapshot().back().type==28);
    std::thread writer([]{for(int i=0;i<5000;++i)log_query("thread.example",false,65,1);});
    for(int i=0;i<5000;++i)CHECK(query_log_snapshot().size()<=100); writer.join();
    CHECK(log_query(std::string(254,'x'),false,1,1)!=ESP_OK);
#else
    CHECK(!query_logging_enabled()); CHECK(query_log_snapshot().empty());
#endif
    httpd_req_t log={"/querylog.json"}; CHECK(registered_routes[0].handler(&log)==ESP_OK);
    cJSON* log_json=cJSON_Parse(log.response.c_str()); CHECK(cJSON_IsArray(log_json));
    CHECK(cJSON_GetArraySize(log_json)<=100); cJSON_Delete(log_json);
}
static void benchmark(const std::filesystem::path& root) {
    fs::test_root(root.string());
    auto begin=std::chrono::steady_clock::now(); initialize_blocklists();
    auto loaded=std::chrono::steady_clock::now(); auto status=blocklist_status(); CHECK(status.valid);
    unsigned hits=0, false_hits=0;
    for(int i=0;i<10000;++i) {
        char domain[64]; snprintf(domain,sizeof(domain),"d%05d.blocked.test",i%35000);
        hits+=in_blacklist(domain);
    }
    auto hit_end=std::chrono::steady_clock::now();
    for(int i=0;i<10000;++i) false_hits+=in_blacklist(("unlisted-"+std::to_string(i)+".invalid").c_str());
    auto miss_end=std::chrono::steady_clock::now();
    CHECK(hits==10000 && false_hits==0);
    auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
    std::cout<<"HOST BENCHMARK: records="<<status.records<<"; bytes="<<status.bytes
      <<"; index metadata="<<sizeof(BlocklistIndex)<<"; validation_ms="<<ms(begin,loaded)
      <<"; hit_mean_us="<<ms(loaded,hit_end)/10<<"; miss_mean_us="<<ms(hit_end,miss_end)/10
      <<"; 10000 hits; 0/10000 false positives (synthetic fixtures, not ESP32 timings)\n";
}
int main(int argc,char** argv) {
    try { if(argc==3) { benchmark(argv[1]);return 0; }
        if(argc!=2)throw std::runtime_error("Provide private fixture directory");
        dns_tests(); forwarding_tests(); storage_tests(argv[1]); list_tests(argv[1]); dashboard_tests();
        std::cout<<"PASS: "<<checks<<" checks; 20,000 malformed fuzz cases; actual firmware logic\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
