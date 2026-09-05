// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco

#include <zap/transport.hpp>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstring>

namespace zap::transport {
namespace {

std::string errno_text(const char* what) {
    return std::string("transport: ") + what + ": " + std::strerror(errno);
}

// read_full loops until n bytes are in, or the peer is gone.
bool read_full(int fd, std::uint8_t* p, std::size_t n) {
    while (n > 0) {
        const ssize_t got = ::read(fd, p, n);
        if (got > 0) {
            p += got;
            n -= static_cast<std::size_t>(got);
            continue;
        }
        if (got < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

// MSG_NOSIGNAL, so writing to a peer that has already hung up is an error
// returned here rather than a signal that kills the process.
bool write_full(int fd, const std::uint8_t* p, std::size_t n) {
    while (n > 0) {
        const ssize_t put = ::send(fd, p, n, MSG_NOSIGNAL);
        if (put > 0) {
            p += put;
            n -= static_cast<std::size_t>(put);
            continue;
        }
        if (put < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

// split_host_port cuts "host:port" at the LAST colon, so a bracketed IPv6
// literal survives.
bool split_host_port(std::string_view addr, std::string& host, std::string& port) {
    const auto colon = addr.rfind(':');
    if (colon == std::string_view::npos) return false;
    host = std::string(addr.substr(0, colon));
    port = std::string(addr.substr(colon + 1));
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
        host = host.substr(1, host.size() - 2);
    return !port.empty();
}

}  // namespace

// ── Conn ───────────────────────────────────────────────────────────────────

Conn::Conn(int fd, rpc::Dispatch dispatch) : fd_(fd), dispatch_(std::move(dispatch)) {}

std::shared_ptr<Conn> Conn::attach(int fd, rpc::Dispatch dispatch) {
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));  // best effort; unix ignores it
    std::shared_ptr<Conn> c(new Conn(fd, std::move(dispatch)));
    c->start();
    return c;
}

void Conn::start() {
    if (dispatch_) {
        workers_.reserve(kMaxInFlight);
        for (std::size_t i = 0; i < kMaxInFlight; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }
    reader_ = std::thread([this] { read_loop(); });
}

// The threads do not hold the Conn alive; the owner does. Destruction closes
// the socket, which wakes the reader, and joins everything before a member can
// go out from under it.
Conn::~Conn() {
    close();
    if (reader_.joinable()) reader_.join();
    for (auto& w : workers_)
        if (w.joinable()) w.join();
    if (fd_ >= 0) ::close(fd_);
}

std::expected<std::shared_ptr<Conn>, std::string> Conn::dial(std::string_view network,
                                                             std::string_view addr,
                                                             rpc::Dispatch dispatch) {
    if (network == "unix") {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return std::unexpected(errno_text("socket"));
        sockaddr_un sa{};
        sa.sun_family = AF_UNIX;
        if (addr.size() >= sizeof(sa.sun_path)) {
            ::close(fd);
            return std::unexpected("transport: unix path too long");
        }
        std::memcpy(sa.sun_path, addr.data(), addr.size());
        if (::connect(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
            const auto err = errno_text("connect");
            ::close(fd);
            return std::unexpected(err);
        }
        return attach(fd, std::move(dispatch));
    }
    if (network != "tcp") return std::unexpected("transport: unknown network");

    std::string host, port;
    if (!split_host_port(addr, host, port)) return std::unexpected("transport: bad address");
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || res == nullptr)
        return std::unexpected("transport: resolve failed");
    for (addrinfo* it = res; it != nullptr; it = it->ai_next) {
        const int fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            ::freeaddrinfo(res);
            return attach(fd, std::move(dispatch));
        }
        ::close(fd);
    }
    ::freeaddrinfo(res);
    return std::unexpected(errno_text("connect"));
}

std::future<Reply> Conn::begin(std::span<const std::uint8_t> envelope) {
    std::promise<Reply> promise;
    std::future<Reply> future = promise.get_future();

    auto call = rpc::parse_request(envelope);
    if (!call) {
        promise.set_value(std::unexpected(std::string(describe(call.error()))));
        return future;
    }
    const std::uint32_t id = call->promise_id;

    {
        std::lock_guard<std::mutex> lock(pending_mu_);
        if (closed_.load()) {
            promise.set_value(std::unexpected("transport: connection closed"));
            return future;
        }
        pending_.emplace(id, std::move(promise));
    }

    if (auto ok = write_frame(kDirRequest, envelope); !ok) {
        std::lock_guard<std::mutex> lock(pending_mu_);
        if (auto it = pending_.find(id); it != pending_.end()) {
            it->second.set_value(std::unexpected(ok.error()));
            pending_.erase(it);
        }
    }
    return future;
}

void Conn::close() {
    if (closed_.exchange(true)) return;
    // Shut the socket down first so a reader parked in read() wakes.
    ::shutdown(fd_, SHUT_RDWR);
    queue_cv_.notify_all();
    queue_drain_.notify_all();
    fail_pending("transport: connection closed");
}

void Conn::fail_pending(const std::string& why) {
    std::map<std::uint32_t, std::promise<Reply>> taken;
    {
        std::lock_guard<std::mutex> lock(pending_mu_);
        taken.swap(pending_);
    }
    for (auto& [id, p] : taken) p.set_value(std::unexpected(why));
}

std::expected<void, std::string> Conn::write_frame(std::uint8_t dir,
                                                   std::span<const std::uint8_t> envelope) {
    std::uint8_t hdr[5];
    store_u32(hdr, static_cast<std::uint32_t>(1 + envelope.size()));
    hdr[4] = dir;

    std::lock_guard<std::mutex> lock(write_mu_);
    if (closed_.load()) return std::unexpected("transport: connection closed");
    if (!write_full(fd_, hdr, sizeof(hdr)) || !write_full(fd_, envelope.data(), envelope.size()))
        return std::unexpected(errno_text("write"));
    return {};
}

void Conn::read_loop() {
    for (;;) {
        std::uint8_t hdr[5];
        if (!read_full(fd_, hdr, sizeof(hdr))) break;
        const std::uint32_t n = load_u32(hdr);
        if (n < 1 || n > kMaxFrame) break;  // a length out of range ends the connection
        std::vector<std::uint8_t> body(n - 1);
        if (!read_full(fd_, body.data(), body.size())) break;

        if (hdr[4] == kDirResponse) {
            auto resp = rpc::parse_response(body);
            if (!resp) continue;  // a malformed answer is dropped, not fatal
            std::promise<Reply> waiter;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(pending_mu_);
                if (auto it = pending_.find(resp->promise_id); it != pending_.end()) {
                    waiter = std::move(it->second);
                    pending_.erase(it);
                    found = true;
                }
            }
            if (found)
                waiter.set_value(Answer{resp->status, resp->promise_id,
                                        std::vector<std::uint8_t>(resp->body.begin(),
                                                                  resp->body.end())});
            continue;
        }
        if (hdr[4] != kDirRequest) break;  // an unknown direction ends the connection
        if (!dispatch_) continue;          // call-only peer: nothing serves this

        std::unique_lock<std::mutex> lock(queue_mu_);
        // At the bound, stop reading. The peer feels it as backpressure, which
        // is the whole point of a bound.
        queue_drain_.wait(lock, [&] { return queue_.size() < kMaxInFlight || closed_.load(); });
        if (closed_.load()) break;
        queue_.push_back(std::move(body));
        lock.unlock();
        queue_cv_.notify_one();
    }
    close();
}

void Conn::worker_loop() {
    for (;;) {
        std::vector<std::uint8_t> envelope;
        {
            std::unique_lock<std::mutex> lock(queue_mu_);
            queue_cv_.wait(lock, [&] { return !queue_.empty() || closed_.load(); });
            if (queue_.empty()) return;  // closed and drained
            envelope = std::move(queue_.front());
            queue_.pop_front();
        }
        queue_drain_.notify_one();
        serve(std::move(envelope));
    }
}

void Conn::serve(std::vector<std::uint8_t> envelope) {
    auto out = dispatch_(envelope);
    if (!out) {
        // Answer anyway, so the caller unblocks instead of waiting out its
        // deadline on a handler that failed.
        auto call = rpc::parse_request(envelope);
        if (!call) return;
        out = rpc::build_response(rpc::kStatusInternal, call->promise_id, {});
    }
    (void)write_frame(kDirResponse, *out);
}

// ── Listener ───────────────────────────────────────────────────────────────

std::expected<Listener, std::string> Listener::bind(std::string_view network,
                                                    std::string_view addr) {
    Listener l;
    if (network == "unix") {
        l.fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (l.fd_ < 0) return std::unexpected(errno_text("socket"));
        sockaddr_un sa{};
        sa.sun_family = AF_UNIX;
        if (addr.size() >= sizeof(sa.sun_path)) return std::unexpected("transport: path too long");
        std::memcpy(sa.sun_path, addr.data(), addr.size());
        ::unlink(sa.sun_path);
        if (::bind(l.fd_, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0)
            return std::unexpected(errno_text("bind"));
        if (::listen(l.fd_, 64) != 0) return std::unexpected(errno_text("listen"));
        l.addr_ = std::string(addr);
        l.path_ = l.addr_;
        return l;
    }
    if (network != "tcp") return std::unexpected("transport: unknown network");

    std::string host, port;
    if (!split_host_port(addr, host, port)) return std::unexpected("transport: bad address");
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* res = nullptr;
    if (::getaddrinfo(host.empty() ? nullptr : host.c_str(), port.c_str(), &hints, &res) != 0 ||
        res == nullptr)
        return std::unexpected("transport: resolve failed");
    l.fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (l.fd_ < 0) {
        ::freeaddrinfo(res);
        return std::unexpected(errno_text("socket"));
    }
    int one = 1;
    ::setsockopt(l.fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    const bool ok = ::bind(l.fd_, res->ai_addr, res->ai_addrlen) == 0 && ::listen(l.fd_, 64) == 0;
    ::freeaddrinfo(res);
    if (!ok) return std::unexpected(errno_text("bind"));

    sockaddr_in bound{};
    socklen_t blen = sizeof(bound);
    if (::getsockname(l.fd_, reinterpret_cast<sockaddr*>(&bound), &blen) != 0)
        return std::unexpected(errno_text("getsockname"));
    char ip[INET_ADDRSTRLEN] = {};
    ::inet_ntop(AF_INET, &bound.sin_addr, ip, sizeof(ip));
    l.addr_ = std::string(ip) + ":" + std::to_string(ntohs(bound.sin_port));
    return l;
}

Listener::Listener(Listener&& other) noexcept
    : fd_(other.fd_), addr_(std::move(other.addr_)), path_(std::move(other.path_)) {
    other.fd_ = -1;
}

Listener& Listener::operator=(Listener&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        addr_ = std::move(other.addr_);
        path_ = std::move(other.path_);
        other.fd_ = -1;
    }
    return *this;
}

Listener::~Listener() { close(); }

void Listener::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (!path_.empty()) {
        ::unlink(path_.c_str());
        path_.clear();
    }
}

std::expected<std::shared_ptr<Conn>, std::string> Listener::accept(rpc::Dispatch dispatch) {
    for (;;) {
        const int fd = ::accept(fd_, nullptr, nullptr);
        if (fd >= 0) return Conn::attach(fd, std::move(dispatch));
        if (errno == EINTR) continue;
        return std::unexpected(errno_text("accept"));
    }
}

}  // namespace zap::transport
