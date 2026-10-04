idf_component_register( SRCS "dns.cpp" "server.cpp" "logging.cpp" "forwarder.cpp" "tcp_transport.cpp" "metrics.cpp"
                        INCLUDE_DIRS "include/"
                        PRIV_REQUIRES error events settings lists)
