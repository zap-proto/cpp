// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// transport.hpp — ZAP over a connection.
//
// This is where ZAP stops being a message layout and becomes a protocol. A
// Conn is SYMMETRIC: both peers may call and both may serve, over the one
// connection, at the same time. And it PIPELINES: begin() ships a request and
// returns immediately, so a caller may have any number of requests in flight,
// and answers arrive in whatever order the peer finishes them — correlation is
// by PromiseID, not by arrival order. A design that waits for each answer
// before sending the next request is not this protocol.
//
// Framing:
//
//   [ u32 len LE ][ 1 byte dir ][ envelope bytes ]      len = 1 + |envelope|
//
// Correlation lives in the envelope (rpc::Call::promise_id /
// rpc::Response::promise_id), so the frame adds no id of its own — it reads
// the id out of the envelope it is already carrying.
//
// A Conn owns its file descriptor, one reader thread, and a bounded pool of
// worker threads that serve inbound requests. The pool is what lets replies
// come back out of order: a slow method does not hold up a fast one behind it.
// At the pool's queue bound the reader stops reading, which is backpressure on
// the peer rather than unbounded growth here.

#pragma once

#include <zap/rpc.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <expected>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace zap::transport {

// Frame direction tags. 3-5 are reserved for the stream frames the Go runtime
// carries (open / msg / end); this runtime speaks the unary half of the
// protocol and refuses the rest rather than half-answering it.
inline constexpr std::uint8_t kDirRequest = 1;
inline constexpr std::uint8_t kDirResponse = 2;

// A single frame's bound, so a corrupt or hostile length word cannot drive an
// unbounded allocation.
inline constexpr std::uint32_t kMaxFrame = 64u << 20;

// How many inbound requests one connection serves at once. Beyond this the
// reader blocks, which is TCP backpressure instead of unbounded threads here.
inline constexpr std::size_t kMaxInFlight = 64;

// Answer is a response with its bytes owned — a future outlives the frame
// buffer the response was read out of, so it cannot be a view.
struct Answer {
    std::uint32_t status = 0;
    std::uint32_t promise_id = 0;
    std::vector<std::uint8_t> body;
};

using Reply = std::expected<Answer, std::string>;

// Conn is one ZAP connection. Construct it with attach() or dial(); it is
// alive until close() or the peer hangs up, and safe for concurrent use.
class Conn {
  public:
    // attach takes ownership of an already-connected socket. dispatch may be
    // empty for a call-only peer, in which case inbound requests are ignored.
    static std::shared_ptr<Conn> attach(int fd, rpc::Dispatch dispatch);

    // dial connects over "tcp" (addr "host:port") or "unix" (addr = path).
    static std::expected<std::shared_ptr<Conn>, std::string> dial(std::string_view network,
                                                                  std::string_view addr,
                                                                  rpc::Dispatch dispatch);

    ~Conn();
    Conn(const Conn&) = delete;
    Conn& operator=(const Conn&) = delete;

    // begin ships a request envelope and returns without waiting. Call it
    // again before the answer arrives — that is the point.
    std::future<Reply> begin(std::span<const std::uint8_t> envelope);

    // call is begin() awaited. It is the convenience, not the model.
    Reply call(std::span<const std::uint8_t> envelope) { return begin(envelope).get(); }

    // next_promise_id hands out a monotonic local id for a caller building
    // envelopes by hand rather than through an rpc::Session.
    std::uint32_t next_promise_id() { return ++promise_id_; }

    bool is_closed() const { return closed_.load(); }
    void close();

  private:
    explicit Conn(int fd, rpc::Dispatch dispatch);
    void start();

    void read_loop();
    void worker_loop();
    void serve(std::vector<std::uint8_t> envelope);
    std::expected<void, std::string> write_frame(std::uint8_t dir,
                                                 std::span<const std::uint8_t> envelope);
    void fail_pending(const std::string& why);

    int fd_ = -1;
    rpc::Dispatch dispatch_;

    std::atomic<bool> closed_{false};
    std::atomic<std::uint32_t> promise_id_{0};

    std::mutex write_mu_;

    std::mutex pending_mu_;
    std::map<std::uint32_t, std::promise<Reply>> pending_;

    std::mutex queue_mu_;
    std::condition_variable queue_cv_;      // work arrived, or the conn closed
    std::condition_variable queue_drain_;   // a slot freed
    std::deque<std::vector<std::uint8_t>> queue_;

    std::thread reader_;
    std::vector<std::thread> workers_;
};

// Listener accepts ZAP connections. Each accepted connection is symmetric like
// any other — the side that accepted may call the side that dialled.
class Listener {
  public:
    static std::expected<Listener, std::string> bind(std::string_view network,
                                                     std::string_view addr);

    Listener(Listener&& other) noexcept;
    Listener& operator=(Listener&& other) noexcept;
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    ~Listener();

    // accept blocks for the next peer and wraps it in a Conn serving dispatch.
    std::expected<std::shared_ptr<Conn>, std::string> accept(rpc::Dispatch dispatch);

    // addr is the address actually bound — the port the kernel chose when the
    // request was port 0.
    const std::string& addr() const { return addr_; }

    void close();

  private:
    Listener() = default;
    int fd_ = -1;
    std::string addr_;
    std::string path_;  // unix socket to unlink on close, empty for tcp
};

}  // namespace zap::transport
