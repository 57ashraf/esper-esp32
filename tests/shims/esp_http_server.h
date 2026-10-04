#pragma once
#include "esp_system.h"
#include <string>
#include <vector>
#include <map>
#include <cstring>
constexpr int HTTP_GET=0, HTTP_POST=1, HTTP_PUT=2, HTTP_DELETE=3;
constexpr int HTTPD_403_FORBIDDEN=403, HTTPD_404_NOT_FOUND=404, HTTPD_500_INTERNAL_SERVER_ERROR=500;
using httpd_handle_t = void*;
struct httpd_req_t {
    const char* uri; int method=HTTP_GET;
    std::string response; int status=200; std::string type;
    std::map<std::string,std::string> headers;
    std::map<std::string,std::string> request_headers = {{"Host", "esper.local"}};
};
struct httpd_uri_t {
    const char* uri=nullptr; int method=0;
    esp_err_t (*handler)(httpd_req_t*)=nullptr;
    void* user_ctx=nullptr;
};
struct httpd_config_t {
    bool (*uri_match_fn)(const char*,const char*,size_t)=nullptr;
    int max_open_sockets=7, recv_wait_timeout=5, send_wait_timeout=5;
};
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
extern std::vector<httpd_uri_t> registered_routes;
inline bool httpd_uri_match_wildcard(const char*,const char*,size_t) { return true; }
inline int httpd_start(httpd_handle_t* server,const httpd_config_t*) { *server=reinterpret_cast<void*>(1);return ESP_OK; }
inline int httpd_stop(httpd_handle_t) { return ESP_OK; }
inline int httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t* r) { registered_routes.push_back(*r);return ESP_OK; }
inline int httpd_resp_set_hdr(httpd_req_t* r,const char* k,const char* v) { r->headers[k]=v;return ESP_OK; }
inline size_t httpd_req_get_hdr_value_len(httpd_req_t* r,const char* k) {
    auto i = r->request_headers.find(k); return i == r->request_headers.end() ? 0 : i->second.size();
}
inline int httpd_req_get_hdr_value_str(httpd_req_t* r,const char* k,char* b,size_t n) {
    auto i = r->request_headers.find(k);
    if (i == r->request_headers.end() || i->second.size()+1 > n) return ESP_FAIL;
    memcpy(b, i->second.c_str(), i->second.size()+1); return ESP_OK;
}
inline int httpd_resp_set_type(httpd_req_t* r,const char* t) { r->type=t;return ESP_OK; }
inline int httpd_resp_send(httpd_req_t* r,const char* s,int n) { r->response=s?std::string(s,n<0?strlen(s):n):"";return ESP_OK; }
inline int httpd_resp_send_err(httpd_req_t* r,int n,const char* s) {r->status=n;r->response=s;return ESP_OK;}
inline int httpd_resp_sendstr_chunk(httpd_req_t* r,const char* s) {if(s)r->response+=s;return ESP_OK;}
inline int httpd_resp_send_chunk(httpd_req_t* r,const char* s,int n) {if(s)r->response.append(s,n);return ESP_OK;}
