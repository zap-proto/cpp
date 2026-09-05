// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// rpc.hpp — the ZAP call envelope, and promise pipelining.
//
// A call carries a caller-assigned PromiseID: the id its answer resolves to.
// A DEPENDENT call sets Target to a prior call's PromiseID, meaning "before
// you dispatch me, substitute the resolved body of the call that answered to
// that id as my payload". The result of A is the input to B, so B never waits
// for A's body to travel back to the caller and out again — both calls ship
// back to back and the server chains them.
//
// Two pieces, each in one place:
//
//   Session   client side. Allocates PromiseIDs and stamps Target onto a
//             dependent call, so a caller can name an answer still in flight.
//   Pipeliner server side. The promise table: resolves Target before dispatch,
//             records every OK answer under its PromiseID, and parks a
//             dependent whose target has not resolved yet until it does.
//
// Both are transport-agnostic — they operate on Call and Response, so the same
// model works in process and over a socket. The wire is unchanged either way:
// Target has always been field @8, and Target = 0 is a plain call, so a peer
// that does not pipeline interoperates by sending zero.
//
// Envelopes, rendered from github.com/zap-proto/go/rpc:
//
//   Request (fixed size 28)
//     method     u32   @0    the interface method's schema ordinal (1-based)
//     promise_id u32   @4    the id this call's answer resolves to
//     target     u32   @8    the promise this call pipelines off (0 = none)
//     cap        bytes @12   opaque capability buffer (may be empty)
//     payload    bytes @20   the encoded method params
//
//   Response (fixed size 20)
//     status     u32   @0    200 ok, else an error code
//     promise_id u32   @4    echoes the request's promise_id
//     body       bytes @12   the encoded results (empty for a void method)
//
// Both are finished with header flags = kMsgTypeRouterBase << 8, so a
// transport routes them to this service's handler (msgType = flags >> 8).

#pragma once

#include <zap/zap.hpp>

#include <condition_variable>
#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace zap::rpc {

// This service's message-type slot, carried in the high byte of the flags word.
inline constexpr std::uint16_t kMsgTypeRouterBase = 200;

// The Target of a call that pipelines off nothing.
inline constexpr std::uint32_t kNoTarget = 0;

inline constexpr std::uint32_t kStatusOK = 200;
inline constexpr std::uint32_t kStatusBadRequest = 400;
inline constexpr std::uint32_t kStatusUnauthorized = 401;
inline constexpr std::uint32_t kStatusForbidden = 403;
inline constexpr std::uint32_t kStatusNotFound = 404;
inline constexpr std::uint32_t kStatusInternal = 500;

inline constexpr std::int64_t kReqMethod = 0;
inline constexpr std::int64_t kReqPromiseID = 4;
inline constexpr std::int64_t kReqTarget = 8;
inline constexpr std::int64_t kReqCap = 12;
inline constexpr std::int64_t kReqPayload = 20;
inline constexpr std::int64_t kReqFixedSize = 28;

inline constexpr std::int64_t kRespStatus = 0;
inline constexpr std::int64_t kRespPromiseID = 4;
inline constexpr std::int64_t kRespBody = 12;
inline constexpr std::int64_t kRespFixedSize = 20;

// Call is one outbound request. cap and payload are views; parse_request
// returns views onto the envelope it was given, so copy them to outlive it.
struct Call {
    std::uint32_t method = 0;
    std::uint32_t promise_id = 0;
    std::uint32_t target = kNoTarget;
    std::span<const std::uint8_t> cap;
    std::span<const std::uint8_t> payload;
};

struct Response {
    std::uint32_t status = 0;
    std::uint32_t promise_id = 0;
    std::span<const std::uint8_t> body;
};

std::vector<std::uint8_t> build_request(const Call& c);
std::expected<Call, zap::Error> parse_request(std::span<const std::uint8_t> envelope);

std::vector<std::uint8_t> build_response(std::uint32_t status, std::uint32_t promise_id,
                                         std::span<const std::uint8_t> body);
std::expected<Response, zap::Error> parse_response(std::span<const std::uint8_t> envelope);

// Dispatch turns a request envelope into a response envelope. It is the shape
// a generated server entry point has, and the shape a transport serves.
using Dispatch =
    std::function<std::expected<std::vector<std::uint8_t>, std::string>(std::span<const std::uint8_t>)>;

// Promise is a handle to an answer still in flight. Pass its id as the Target
// of the next call instead of threading raw integers by hand.
struct Promise {
    std::uint32_t id = kNoTarget;
};

// Session is the client half of pipelining: a monotonic PromiseID allocator
// scoped to one connection. Ids are unique and never zero.
class Session {
  public:
    Promise next();

    // origin builds the first call of a pipeline: fresh id, no target.
    Call origin(Promise p, std::uint32_t method, std::span<const std::uint8_t> cap,
                std::span<const std::uint8_t> payload) const;

    // pipeline builds a dependent call: its own fresh id, and target's id as
    // Target. The server substitutes target's resolved body for this call's
    // payload before dispatch, so payload here is only the part of the request
    // the upstream answer does not supply — often nothing.
    Call pipeline(Promise p, Promise target, std::uint32_t method,
                  std::span<const std::uint8_t> cap,
                  std::span<const std::uint8_t> payload) const;

  private:
    mutable std::mutex mu_;
    std::uint32_t next_ = 0;
};

// Pipeliner is the server-side promise table for one connection. Construct it
// around a dispatch and feed it every inbound request envelope; it answers
// straight through for a plain call, resolves a dependent whose target is
// known, and parks one whose target is still in flight.
//
// Safe for concurrent handle() from several threads, which is what lets a
// transport dispatch pipelined requests in parallel.
class Pipeliner {
  public:
    explicit Pipeliner(Dispatch dispatch) : dispatch_(std::move(dispatch)) {}

    std::expected<std::vector<std::uint8_t>, std::string> handle(
        std::span<const std::uint8_t> envelope);

    // finish drops the cached answer for id once the caller knows nothing more
    // will pipeline on it. It is optional — without it a Pipeliner keeps each
    // OK answer for the connection's life, so a dependent that arrives late
    // still finds its target — but a long-lived connection that pipelines
    // heavily should finish each promise to bound the table.
    //
    // After finish, id is terminal: any dependent naming it, parked or later,
    // is refused rather than hung.
    void finish(std::uint32_t id);

  private:
    struct Parked {
        std::uint32_t promise_id = 0;
        std::uint32_t method = 0;
        std::vector<std::uint8_t> cap;
        std::vector<std::uint8_t> payload;
    };

    struct OwnedCall {
        std::uint32_t method = 0;
        std::uint32_t promise_id = 0;
        std::uint32_t target = kNoTarget;
        std::vector<std::uint8_t> cap;
        std::vector<std::uint8_t> payload;
    };

    std::expected<std::vector<std::uint8_t>, std::string> run(const OwnedCall& c);
    void record(std::uint32_t id, std::uint32_t status, std::span<const std::uint8_t> body);

    Dispatch dispatch_;

    std::mutex mu_;
    std::condition_variable cv_;
    std::map<std::uint32_t, std::vector<std::uint8_t>> resolved_;  // id -> OK body
    std::set<std::uint32_t> refused_;   // answered non-OK, or dispatch failed
    std::set<std::uint32_t> finished_;  // answer dropped on purpose
};

}  // namespace zap::rpc
