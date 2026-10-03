#include "webserver.h"
#include "get_handlers.h"
#include "esp_http_server.h"
esp_err_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_open_sockets = 3;
    config.recv_wait_timeout = 3; config.send_wait_timeout = 3;
    httpd_handle_t server = nullptr;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) return err;
    err = register_get_handlers(server);
    if (err != ESP_OK) httpd_stop(server);
    return err;
}
