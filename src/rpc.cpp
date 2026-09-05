// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco

#include <zap/rpc.hpp>

namespace zap::rpc {

std::vector<std::uint8_t> build_request(const Call& c) {
    Builder b(c.cap.size() + c.payload.size() + static_cast<std::size_t>(kReqFixedSize) + 64,
              kVersion2);
    auto ob = b.start_object(kReqFixedSize);
    ob.set_u32(kReqMethod, c.method);
    ob.set_u32(kReqPromiseID, c.promise_id);
    ob.set_u32(kReqTarget, c.target);
    ob.set_bytes(kReqCap, c.cap);
    ob.set_bytes(kReqPayload, c.payload);
    ob.finish_as_root();
    return b.finish_with_flags(static_cast<std::uint16_t>(kMsgTypeRouterBase << 8));
}

std::expected<Call, zap::Error> parse_request(std::span<const std::uint8_t> envelope) {
    auto m = Message::parse(envelope);
    if (!m) return std::unexpected(m.error());
    const Object r = m->root();
    Call c;
    c.method = r.u32(kReqMethod);
    c.promise_id = r.u32(kReqPromiseID);
    c.target = r.u32(kReqTarget);
    c.cap = r.bytes(kReqCap);
    c.payload = r.bytes(kReqPayload);
    return c;
}

std::vector<std::uint8_t> build_response(std::uint32_t status, std::uint32_t promise_id,
                                         std::span<const std::uint8_t> body) {
    Builder b(body.size() + static_cast<std::size_t>(kRespFixedSize) + 64, kVersion2);
    auto ob = b.start_object(kRespFixedSize);
    ob.set_u32(kRespStatus, status);
    ob.set_u32(kRespPromiseID, promise_id);
    ob.set_bytes(kRespBody, body);
    ob.finish_as_root();
    return b.finish_with_flags(static_cast<std::uint16_t>(kMsgTypeRouterBase << 8));
}

std::expected<Response, zap::Error> parse_response(std::span<const std::uint8_t> envelope) {
    auto m = Message::parse(envelope);
    if (!m) return std::unexpected(m.error());
    const Object r = m->root();
    Response resp;
    resp.status = r.u32(kRespStatus);
    resp.promise_id = r.u32(kRespPromiseID);
    resp.body = r.bytes(kRespBody);
    return resp;
}

// ── Session ────────────────────────────────────────────────────────────────

Promise Session::next() {
    std::lock_guard<std::mutex> lock(mu_);
    ++next_;
    if (next_ == kNoTarget) ++next_;  // wrapped past 2^32-1 back to zero; skip it
    return Promise{next_};
}

Call Session::origin(Promise p, std::uint32_t method, std::span<const std::uint8_t> cap,
                     std::span<const std::uint8_t> payload) const {
    return Call{method, p.id, kNoTarget, cap, payload};
}

Call Session::pipeline(Promise p, Promise target, std::uint32_t method,
                       std::span<const std::uint8_t> cap,
                       std::span<const std::uint8_t> payload) const {
    return Call{method, p.id, target.id, cap, payload};
}

// ── Pipeliner ──────────────────────────────────────────────────────────────

std::expected<std::vector<std::uint8_t>, std::string> Pipeliner::handle(
    std::span<const std::uint8_t> envelope) {
    auto parsed = parse_request(envelope);
    if (!parsed) return std::unexpected(std::string(describe(parsed.error())));

    OwnedCall c;
    c.method = parsed->method;
    c.promise_id = parsed->promise_id;
    c.target = parsed->target;
    c.cap.assign(parsed->cap.begin(), parsed->cap.end());
    c.payload.assign(parsed->payload.begin(), parsed->payload.end());

    if (c.target == kNoTarget) return run(c);

    // A dependent call. Wait for its target to become knowable: resolved (use
    // its body as our payload), refused, or finished (both terminal — the
    // target can never produce a result, so refuse instead of hanging).
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [&] {
        return resolved_.count(c.target) != 0 || refused_.count(c.target) != 0 ||
               finished_.count(c.target) != 0;
    });
    if (auto it = resolved_.find(c.target); it != resolved_.end()) {
        c.payload = it->second;
        lock.unlock();
        return run(c);
    }
    lock.unlock();
    return build_response(kStatusBadRequest, c.promise_id, {});
}

std::expected<std::vector<std::uint8_t>, std::string> Pipeliner::run(const OwnedCall& c) {
    const Call call{c.method, c.promise_id, c.target, c.cap, c.payload};
    auto env = build_request(call);
    auto out = dispatch_(env);
    if (!out) {
        // A dispatch failure poisons this id: no dependent on it can resolve.
        record(c.promise_id, kStatusInternal, {});
        return out;
    }
    auto resp = parse_response(*out);
    if (!resp) {
        record(c.promise_id, kStatusInternal, {});
        return std::unexpected(std::string(describe(resp.error())));
    }
    record(c.promise_id, resp->status, resp->body);
    return out;
}

void Pipeliner::record(std::uint32_t id, std::uint32_t status,
                       std::span<const std::uint8_t> body) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (finished_.count(id) != 0) return;  // the caller already dropped this answer
        if (status == kStatusOK) {
            resolved_[id].assign(body.begin(), body.end());
        } else {
            refused_.insert(id);
        }
    }
    cv_.notify_all();
}

void Pipeliner::finish(std::uint32_t id) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        resolved_.erase(id);
        refused_.erase(id);
        finished_.insert(id);
    }
    cv_.notify_all();
}

}  // namespace zap::rpc
