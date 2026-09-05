// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// transport_test.cpp — the protocol, not the bytes.
//
// Two properties are what make ZAP a protocol rather than a framing, and each
// is proved here without leaning on a timer:
//
//   BIDIRECTIONAL  both peers initiate. The side that accepted calls the side
//                  that dialled, while that side is mid-call itself.
//   PIPELINING     many requests in flight at once, and answers that come back
//                  in whatever order the peer finishes them. The server here
//                  refuses to answer ANY request until it has received all of
//                  them, so the test can only finish if they were genuinely in
//                  flight together; and it then answers in reverse, so the
//                  answer to the last request provably arrives before the
//                  answer to the first.

#include "check.hpp"

#include <zap/transport.hpp>

#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace zap;
using namespace std::chrono_literals;

namespace {

std::span<const std::uint8_t> view(const std::vector<std::uint8_t>& v) { return {v.data(), v.size()}; }
std::span<const std::uint8_t> literal(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}
std::string text(std::span<const std::uint8_t> b) {
    return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

// A dispatch that answers with a fixed name and the payload it was given, so a
// caller can tell WHICH peer answered.
rpc::Dispatch answers(std::string name) {
    return [name](std::span<const std::uint8_t> env)
               -> std::expected<std::vector<std::uint8_t>, std::string> {
        auto c = rpc::parse_request(env);
        if (!c) return std::unexpected(std::string("bad request"));
        const std::string body = name + ":" + text(c->payload);
        return rpc::build_response(rpc::kStatusOK, c->promise_id, literal(body));
    };
}

// ── BOTH PEERS INITIATE ────────────────────────────────────────────────────
//
// One connection, two Conns, each serving. A calls B and B calls A, and each
// side's own call is outstanding while it answers the other's — so this is not
// two half-duplex exchanges taking turns.
void bidirectional() {
    int fds[2];
    CHECK_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    // Each side's handler parks until the OTHER side's request has arrived, so
    // neither can finish alone: the connection has to carry both directions at
    // once or this deadlocks and the test fails by timing out.
    std::mutex mu;
    std::condition_variable cv;
    int arrived = 0;
    auto gate = [&](std::string name) -> rpc::Dispatch {
        return [&, name](std::span<const std::uint8_t> env)
                   -> std::expected<std::vector<std::uint8_t>, std::string> {
            auto c = rpc::parse_request(env);
            if (!c) return std::unexpected(std::string("bad request"));
            {
                std::unique_lock<std::mutex> lock(mu);
                ++arrived;
                cv.notify_all();
                cv.wait(lock, [&] { return arrived == 2; });
            }
            const std::string body = name + ":" + text(c->payload);
            return rpc::build_response(rpc::kStatusOK, c->promise_id, literal(body));
        };
    };

    auto a = transport::Conn::attach(fds[0], gate("A"));
    auto b = transport::Conn::attach(fds[1], gate("B"));

    rpc::Session sa, sb;
    auto fa = a->begin(view(rpc::build_request(sa.origin(sa.next(), 1, {}, literal("ping")))));
    auto fb = b->begin(view(rpc::build_request(sb.origin(sb.next(), 1, {}, literal("pong")))));

    auto ra = fa.get();
    auto rb = fb.get();
    CHECK(ra.has_value());
    CHECK(rb.has_value());
    // A's call was answered by B, and B's by A. The direction is in the answer.
    CHECK_EQ(text(ra->body), std::string("B:ping"));
    CHECK_EQ(text(rb->body), std::string("A:pong"));
    CHECK_EQ(ra->status, rpc::kStatusOK);
}

// ── MANY IN FLIGHT, ANSWERS OUT OF ORDER ───────────────────────────────────
void pipelining() {
    constexpr int kN = 8;
    constexpr std::uint32_t kRelease = 100;  // the method that unblocks request 0

    int fds[2];
    CHECK_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::mutex mu;
    std::condition_variable cv;
    int seen = 0;        // requests received so far
    int turn = kN - 1;   // whose turn it is to answer; counts DOWN
    bool released = false;

    auto server = [&](std::span<const std::uint8_t> env)
        -> std::expected<std::vector<std::uint8_t>, std::string> {
        auto c = rpc::parse_request(env);
        if (!c) return std::unexpected(std::string("bad request"));

        if (c->method == kRelease) {
            {
                std::lock_guard<std::mutex> lock(mu);
                released = true;
            }
            cv.notify_all();
            return rpc::build_response(rpc::kStatusOK, c->promise_id, literal("released"));
        }

        const int i = static_cast<int>(c->method) - 1;
        std::unique_lock<std::mutex> lock(mu);
        // Answer nothing until every request has arrived. Were the caller
        // waiting for each answer before sending the next, this never releases
        // and the test hangs instead of passing.
        ++seen;
        cv.notify_all();
        cv.wait(lock, [&] { return seen == kN; });
        // Then answer in reverse: the last request asked is the first answered.
        cv.wait(lock, [&] { return turn == i; });
        // The first request asked is answered last, and not even then until the
        // caller says so — which it only says after it has the last one's
        // answer in hand.
        if (i == 0) cv.wait(lock, [&] { return released; });
        --turn;
        cv.notify_all();
        lock.unlock();

        const std::string body = "answer-" + std::to_string(i);
        return rpc::build_response(rpc::kStatusOK, c->promise_id, literal(body));
    };

    auto client = transport::Conn::attach(fds[0], nullptr);
    auto serving = transport::Conn::attach(fds[1], server);

    rpc::Session s;
    std::vector<std::future<transport::Reply>> waiting;
    std::vector<std::uint32_t> ids;
    for (int i = 0; i < kN; ++i) {
        const auto p = s.next();
        ids.push_back(p.id);
        // No get() in this loop: every request ships before any answer is read.
        waiting.push_back(client->begin(
            view(rpc::build_request(s.origin(p, static_cast<std::uint32_t>(i + 1), {}, {})))));
    }

    // The answer to the LAST request comes back first.
    auto last = waiting[kN - 1].get();
    CHECK(last.has_value());
    CHECK_EQ(text(last->body), std::string("answer-7"));
    CHECK_EQ(last->promise_id, ids[kN - 1]);

    // And at that moment the FIRST request is provably still unanswered: its
    // handler is parked waiting for a release this caller has not sent yet. So
    // answer 7 came back ahead of answer 0 — out of order, not merely late.
    CHECK_EQ(waiting[0].wait_for(0s), std::future_status::timeout);

    // Sending that release is itself a call, served on the same connection
    // while eight others are still in flight on it.
    auto rel = client->call(view(rpc::build_request(s.origin(s.next(), kRelease, {}, {}))));
    CHECK(rel.has_value());
    CHECK_EQ(text(rel->body), std::string("released"));

    // Every answer lands, correlated to its own request by PromiseID.
    for (int i = kN - 2; i >= 0; --i) {
        auto r = waiting[i].get();
        CHECK(r.has_value());
        CHECK_EQ(r->promise_id, ids[i]);
        CHECK_EQ(text(r->body), "answer-" + std::to_string(i));
    }
}

// ── over a real socket, dialled and accepted ───────────────────────────────
void over_a_listener() {
    auto listener = transport::Listener::bind("tcp", "127.0.0.1:0");
    CHECK(listener.has_value());

    std::shared_ptr<transport::Conn> served;
    std::thread accepting([&] {
        auto c = listener->accept(answers("server"));
        if (c) served = *c;
    });

    auto client = transport::Conn::dial("tcp", listener->addr(), answers("client"));
    CHECK(client.has_value());
    accepting.join();
    CHECK(served != nullptr);

    rpc::Session s;
    auto r = (*client)->call(
        view(rpc::build_request(s.origin(s.next(), 1, {}, literal("hello")))));
    CHECK(r.has_value());
    CHECK_EQ(text(r->body), std::string("server:hello"));

    // The side that ACCEPTED calls the side that dialled. Nothing about the
    // connection remembers who started it.
    rpc::Session back;
    auto r2 = served->call(
        view(rpc::build_request(back.origin(back.next(), 1, {}, literal("who")))));
    CHECK(r2.has_value());
    CHECK_EQ(text(r2->body), std::string("client:who"));
}

// A handler that fails still produces an answer, so a caller unblocks instead
// of waiting out a deadline on a call nobody will ever answer.
void a_failed_handler_still_answers() {
    int fds[2];
    CHECK_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    auto client = transport::Conn::attach(fds[0], nullptr);
    auto server = transport::Conn::attach(
        fds[1], [](std::span<const std::uint8_t>)
                    -> std::expected<std::vector<std::uint8_t>, std::string> {
            return std::unexpected(std::string("handler exploded"));
        });

    rpc::Session s;
    auto r = client->call(view(rpc::build_request(s.origin(s.next(), 1, {}, {}))));
    CHECK(r.has_value());
    CHECK_EQ(r->status, rpc::kStatusInternal);
}

// Closing fails every call still in flight rather than leaving it parked.
void close_fails_the_outstanding() {
    int fds[2];
    CHECK_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    auto client = transport::Conn::attach(fds[0], nullptr);
    auto silent = transport::Conn::attach(fds[1], nullptr);  // never answers

    rpc::Session s;
    auto f = client->begin(view(rpc::build_request(s.origin(s.next(), 1, {}, {}))));
    client->close();
    auto r = f.get();
    CHECK(!r.has_value());
    CHECK(client->is_closed());
}

// A peer hanging up ends the connection here too.
void peer_hangup_is_noticed() {
    int fds[2];
    CHECK_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    auto client = transport::Conn::attach(fds[0], nullptr);
    ::close(fds[1]);

    rpc::Session s;
    auto r = client->call(view(rpc::build_request(s.origin(s.next(), 1, {}, {}))));
    CHECK(!r.has_value());
}

}  // namespace

int main() {
    bidirectional();
    pipelining();
    over_a_listener();
    a_failed_handler_still_answers();
    close_fails_the_outstanding();
    peer_hangup_is_noticed();
    return zaptest::verdict("transport");
}
