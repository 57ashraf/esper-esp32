#include "dns/tcp_transport.h"
#include "esp_timer.h"
#include <cerrno>
#include <fcntl.h>
#if defined(ESPER_HOST_TEST) && defined(_WIN32)
using Socket = SOCKET;
static void socket_close(Socket s) { closesocket(s); }
static int socket_error() { return WSAGetLastError(); }
static bool retry_error(int e) { return e == WSAEWOULDBLOCK || e == WSAEINTR; }
static bool in_progress(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
static bool nonblocking(Socket s) { u_long yes = 1; return ioctlsocket(s, FIONBIO, &yes) == 0; }
#else
using Socket = int;
static void socket_close(Socket s) { close(s); }
static int socket_error() { return errno; }
static bool retry_error(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINTR; }
static bool in_progress(int e) { return e == EINPROGRESS; }
static bool nonblocking(Socket s) { return fcntl(s, F_SETFL, O_NONBLOCK) == 0; }
#endif
struct SocketGuard {
    Socket value;
    ~SocketGuard() { socket_close(value); }
};
static bool ready(Socket s, bool write, int64_t deadline) {
    for (;;) {
        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) return false;
        timeval tv = {}; tv.tv_sec = remaining / 1000000; tv.tv_usec = remaining % 1000000;
        fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
        int n = select(static_cast<int>(s)+1, write ? nullptr : &fds, write ? &fds : nullptr, nullptr, &tv);
        if (n > 0) return true;
        if (n == 0 || !retry_error(socket_error())) return false;
    }
}
static bool transfer(Socket s, uint8_t* bytes, size_t size, bool write, int64_t deadline) {
    size_t offset = 0;
    while (offset < size) {
        if (!ready(s, write, deadline)) return false;
        // POSIX tests suppress SIGPIPE; ESP-IDF/lwIP and Winsock do not use it.
#ifdef MSG_NOSIGNAL
        constexpr int send_flags = MSG_NOSIGNAL;
#else
        constexpr int send_flags = 0;
#endif
        int n = write ? send(s, reinterpret_cast<const char*>(bytes+offset), size-offset, send_flags)
                      : recv(s, reinterpret_cast<char*>(bytes+offset), size-offset, 0);
        if (n < 0 && retry_error(socket_error())) continue;
        if (n <= 0) return false;
        offset += static_cast<size_t>(n);
    }
    return true;
}
bool dns_tcp_exchange(const sockaddr_in& upstream, const std::vector<uint8_t>& query,
                      std::vector<uint8_t>& response, int64_t timeout_us) {
    response.clear();
    if (query.size() < 12 || query.size() > MAX_PACKET_SIZE || timeout_us <= 0 || timeout_us > 3000000)
        return false;
    Socket s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == static_cast<Socket>(-1)) return false;
    SocketGuard guard{s}; // also closes on allocation exceptions
    int64_t deadline = esp_timer_get_time() + timeout_us;
    try {
        if (!nonblocking(s)) return false;
        if (connect(s, reinterpret_cast<const sockaddr*>(&upstream), sizeof(upstream)) < 0 &&
            !in_progress(socket_error())) return false;
        int error = 0; socklen_t size = sizeof(error);
        if (!ready(s, true, deadline) ||
            getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) < 0 || error)
            return false;
        uint8_t length[2] = {static_cast<uint8_t>(query.size() >> 8), static_cast<uint8_t>(query.size())};
        if (!transfer(s, length, 2, true, deadline) ||
            !transfer(s, const_cast<uint8_t*>(query.data()), query.size(), true, deadline)) return false;
        if (!transfer(s, length, 2, false, deadline)) return false;
        size_t n = (static_cast<size_t>(length[0]) << 8) | length[1];
        if (n < 12 || n > MAX_PACKET_SIZE) return false;
        response.resize(n);
        if (!transfer(s, response.data(), n, false, deadline)) { response.clear(); return false; }
        return true;
    } catch (...) { response.clear(); return false; }
}
