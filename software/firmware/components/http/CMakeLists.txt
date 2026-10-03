idf_component_register(SRCS "webserver.cpp" "get_handlers.cpp" "safe_dashboard.cpp"
    INCLUDE_DIRS "include/" PRIV_REQUIRES dns settings lists events json esp_http_server)
