// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// rpc_test.cpp — the call envelope, against the canonical runtime's bytes, and
// the promise table that makes a dependent call not wait.

#include "check.hpp"

#include <zap/rpc.hpp>

#include <cctype>
#include <string>
#include <thread>

using namespace zap;
using zaptest::unhex;

namespace {

std::span<const std::uint8_t> view(const std::vector<std::uint8_t>& v) { return {v.data(), v.size()}; }
std::span<const std::uint8_t> literal(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}
std::string text(std::span<const std::uint8_t> b) {
    return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

// The four envelope shapes, byte for byte out of github.com/zap-proto/go/rpc.
void envelope_bytes() {
    const std::uint8_t cap[] = {0xaa, 0xbb};
    CHECK_HEX(view(rpc::build_request(rpc::Call{3, 7, rpc::kNoTarget, cap, literal("params")})),
              "5a415000020000c8100000003400000003000000070000000000000010000000020000000a0000000600"
              "0000aabb706172616d73");
    CHECK_HEX(view(rpc::build_request(rpc::Call{4, 8, 7, {}, {}})),
              "5a415000020000c8100000002c00000004000000080000000700000000000000000000000000000000"
              "000000");
    CHECK_HEX(view(rpc::build_response(rpc::kStatusOK, 7, literal("result"))),
              "5a415000020000c8100000002a000000c800000007000000000000000800000006000000726573756c74");
    CHECK_HEX(view(rpc::build_response(rpc::kStatusNotFound, 9, {})),
              "5a415000020000c810000000240000009401000009000000000000000000000000000000");
}

// The msgType a transport routes on lives in the high byte of the flags word.
void routing_flags() {
    const auto env = rpc::build_request(rpc::Call{1, 1, rpc::kNoTarget, {}, {}});
    auto m = Message::parse(view(env));
    CHECK(m.has_value());
    CHECK_EQ(m->flags() >> 8, rpc::kMsgTypeRouterBase);
    CHECK_EQ(m->version(), kVersion2);
}

void round_trip() {
    const std::uint8_t cap[] = {1, 2, 3};
    const auto env = rpc::build_request(rpc::Call{9, 11, 5, cap, literal("body")});
    auto c = rpc::parse_request(view(env));
    CHECK(c.has_value());
    CHECK_EQ(c->method, 9u);
    CHECK_EQ(c->promise_id, 11u);
    CHECK_EQ(c->target, 5u);
    CHECK_EQ(c->cap.size(), 3u);
    CHECK_EQ(text(c->payload), std::string("body"));

    const auto renv = rpc::build_response(rpc::kStatusForbidden, 11, literal("no"));
    auto r = rpc::parse_response(view(renv));
    CHECK(r.has_value());
    CHECK_EQ(r->status, rpc::kStatusForbidden);
    CHECK_EQ(r->promise_id, 11u);
    CHECK_EQ(text(r->body), std::string("no"));

    // A buffer that is not a message says so rather than answering zeros.
    CHECK(!rpc::parse_request(literal("NOTZAP__________")).has_value());
}

// Session hands out unique, non-zero ids and stamps Target onto a dependent.
void session_ids() {
    rpc::Session s;
    const auto a = s.next();
    const auto b = s.next();
    CHECK_EQ(a.id, 1u);
    CHECK_EQ(b.id, 2u);
    CHECK(a.id != rpc::kNoTarget);

    const auto origin = s.origin(a, 1, {}, literal("in"));
    CHECK_EQ(origin.target, rpc::kNoTarget);
    const auto dep = s.pipeline(b, a, 2, {}, {});
    CHECK_EQ(dep.target, a.id);
    CHECK_EQ(dep.promise_id, b.id);
}

// A dispatch that upper-cases its payload, so a dependent call's result shows
// whether the upstream body really was substituted for its payload.
rpc::Dispatch shouting() {
    return [](std::span<const std::uint8_t> env)
               -> std::expected<std::vector<std::uint8_t>, std::string> {
        auto c = rpc::parse_request(env);
        if (!c) return std::unexpected(std::string("bad request"));
        if (c->method == 99)
            return rpc::build_response(rpc::kStatusNotFound, c->promise_id, {});
        std::string out = text(c->payload);
        for (char& ch : out) ch = static_cast<char>(std::toupper(ch));
        out += "!";
        return rpc::build_response(rpc::kStatusOK, c->promise_id, literal(out));
    };
}

// The result of A is the input to B without B ever carrying it.
void pipelines_on_a_resolved_answer() {
    rpc::Pipeliner p(shouting());
    rpc::Session s;
    const auto a = s.next();
    const auto b = s.next();

    auto ra = p.handle(view(rpc::build_request(s.origin(a, 1, {}, literal("one")))));
    CHECK(ra.has_value());
    CHECK_EQ(text(rpc::parse_response(view(*ra))->body), std::string("ONE!"));

    auto rb = p.handle(view(rpc::build_request(s.pipeline(b, a, 1, {}, {}))));
    CHECK(rb.has_value());
    CHECK_EQ(text(rpc::parse_response(view(*rb))->body), std::string("ONE!!"));
}

// The dependent legitimately arrives FIRST — that is the point of pipelining,
// the two calls ship back to back. It parks until its target resolves.
void parks_until_its_target_arrives() {
    rpc::Pipeliner p(shouting());
    rpc::Session s;
    const auto a = s.next();
    const auto b = s.next();

    std::expected<std::vector<std::uint8_t>, std::string> rb;
    const auto dep = rpc::build_request(s.pipeline(b, a, 1, {}, {}));
    std::thread first([&] { rb = p.handle(view(dep)); });

    const auto origin = rpc::build_request(s.origin(a, 1, {}, literal("two")));
    auto ra = p.handle(view(origin));
    first.join();

    CHECK(ra.has_value());
    CHECK(rb.has_value());
    CHECK_EQ(text(rpc::parse_response(view(*ra))->body), std::string("TWO!"));
    CHECK_EQ(text(rpc::parse_response(view(*rb))->body), std::string("TWO!!"));
}

// A target that answered non-OK can never produce a result, so its dependents
// are refused rather than hung. The same goes for one already finished.
void refuses_a_target_that_cannot_resolve() {
    rpc::Pipeliner p(shouting());
    rpc::Session s;
    const auto a = s.next();
    const auto b = s.next();

    auto ra = p.handle(view(rpc::build_request(s.origin(a, 99, {}, {}))));  // 99 answers 404
    CHECK(ra.has_value());
    CHECK_EQ(rpc::parse_response(view(*ra))->status, rpc::kStatusNotFound);

    auto rb = p.handle(view(rpc::build_request(s.pipeline(b, a, 1, {}, {}))));
    CHECK(rb.has_value());
    CHECK_EQ(rpc::parse_response(view(*rb))->status, rpc::kStatusBadRequest);

    const auto c = s.next();
    auto rc = p.handle(view(rpc::build_request(s.origin(c, 1, {}, literal("x")))));
    CHECK(rc.has_value());
    p.finish(c.id);
    const auto d = s.next();
    auto rd = p.handle(view(rpc::build_request(s.pipeline(d, c, 1, {}, {}))));
    CHECK(rd.has_value());
    CHECK_EQ(rpc::parse_response(view(*rd))->status, rpc::kStatusBadRequest);
}

// finish wakes a dependent already parked, so it is refused instead of waiting
// for an answer that has been dropped.
void finish_releases_a_parked_dependent() {
    rpc::Pipeliner p(shouting());
    rpc::Session s;
    const auto a = s.next();
    const auto b = s.next();

    std::expected<std::vector<std::uint8_t>, std::string> rb;
    const auto dep = rpc::build_request(s.pipeline(b, a, 1, {}, {}));
    std::thread parked([&] { rb = p.handle(view(dep)); });
    p.finish(a.id);
    parked.join();

    CHECK(rb.has_value());
    CHECK_EQ(rpc::parse_response(view(*rb))->status, rpc::kStatusBadRequest);
}

}  // namespace

int main() {
    envelope_bytes();
    routing_flags();
    round_trip();
    session_ids();
    pipelines_on_a_resolved_answer();
    parks_until_its_target_arrives();
    refuses_a_target_that_cannot_resolve();
    finish_releases_a_parked_dependent();
    return zaptest::verdict("rpc");
}
