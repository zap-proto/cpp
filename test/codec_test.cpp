// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// codec_test.cpp — the message layer, against the canonical runtime.
//
// Every hex string in the KAT block came out of github.com/zap-proto/go
// itself: the same calls to the same builder, printed. They are the evidence
// that this runtime writes THE wire and not merely A wire. A round-trip test
// proves an implementation agrees with itself, and self-agreement is exactly
// what a fork also has.

#include "check.hpp"

#include <zap/zap.hpp>

using namespace zap;
using zaptest::unhex;

namespace {

std::span<const std::uint8_t> view(const std::vector<std::uint8_t>& v) { return {v.data(), v.size()}; }
std::span<const std::uint8_t> literal(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

// ── the builder writes the reference's bytes ───────────────────────────────

void scalars() {
    Builder b(64);
    auto ob = b.start_object(24);
    ob.set_u8(0, 0x2a);
    ob.set_u16(2, 0xbeef);
    ob.set_u32(4, 0xdeadbeef);
    ob.set_u64(8, 0x0102030405060708);
    ob.set_bool(16, true);
    ob.finish_as_root();
    CHECK_HEX(view(b.finish()),
              "5a4150000200000010000000280000002a00efbeefbeadde08070605040302010100000000000000");
}

void one_tail() {
    Builder b(64);
    auto ob = b.start_object(16);
    ob.set_u32(0, 7);
    ob.set_bytes(8, literal("hello zap"));
    ob.finish_as_root();
    CHECK_HEX(view(b.finish()),
              "5a4150000200000010000000290000000700000000000000080000000900000068656c6c6f207a6170");
}

void two_tails() {
    Builder b(64);
    auto ob = b.start_object(24);
    ob.set_bytes(0, literal("first"));
    ob.set_bytes(8, literal("second"));
    ob.set_u64(16, 99);
    ob.finish_as_root();
    CHECK_HEX(view(b.finish()),
              "5a41500002000000100000003300000018000000050000001500000006000000630000000000000066"
              "697273747365636f6e64");
}

// A tail set BEFORE a list is started on the same builder. This is the one
// case where holding the payload until finish() and writing it on the spot
// disagree: the reference lays the list down first and the tail after it. An
// implementation that writes the tail immediately produces a different message
// for the same calls — same fields, different bytes, and on a chain that is a
// fork.
void tail_then_list() {
    Builder b(128);
    auto ob = b.start_object(24);
    ob.set_bytes(0, literal("tail"));
    auto lb = b.start_list(4);
    lb.add_u32(1);
    lb.add_u32(2);
    lb.add_u32(3);
    const auto [off, n] = lb.finish();
    ob.set_list(8, off, n);
    ob.set_u64(16, 5);
    ob.finish_as_root();
    CHECK_HEX(view(b.finish()),
              "5a4150000200000010000000380000002400000004000000100000000300000005000000000000000100"
              "000002000000030000007461696c");
}

// A child finalized before its parent lives EARLIER in the segment, so the
// parent's pointer runs backwards. The reader must read it as signed.
void nested() {
    Builder b(128);
    auto child = b.start_object(8);
    child.set_u64(0, 0xcafebabe);
    const auto coff = child.finish();
    auto parent = b.start_object(16);
    parent.set_u32(0, 1);
    parent.set_object(4, coff);
    parent.finish_as_root();
    const auto bytes = b.finish();
    CHECK_HEX(view(bytes),
              "5a415000020000001800000028000000bebafeca0000000001000000f4ffffff0000000000000000");

    auto m = Message::parse(view(bytes));
    CHECK(m.has_value());
    CHECK_EQ(m->root().u32(0), 1u);
    CHECK_EQ(m->root().object(4).u64(0), 0xcafebabeull);
}

void text_empty_list() {
    Builder b(128);
    auto lb = b.start_list(1);
    lb.add_u8(9);
    lb.add_u8(8);
    lb.add_u8(7);
    const auto [off, n] = lb.finish();
    auto ob = b.start_object(24);
    ob.set_text(0, "zap");
    ob.set_bytes(8, {});
    ob.set_list(16, off, n);
    ob.finish_as_root();
    const auto bytes = b.finish();
    CHECK_HEX(view(bytes),
              "5a415000020000001800000033000000090807000000000018000000030000000000000000000000e8ff"
              "ffff030000007a6170");

    auto m = Message::parse(view(bytes));
    CHECK(m.has_value());
    CHECK_EQ(m->root().text(0), std::string_view("zap"));
    CHECK(m->root().bytes(8).empty());
    const auto l = m->root().list(16);
    CHECK_EQ(l.size(), 3);
    CHECK_EQ(l.u8(0), 9);
    CHECK_EQ(l.u8(2), 7);
}

void write_bytes() {
    Builder b(64);
    const auto blob = b.write_bytes(std::vector<std::uint8_t>{1, 2, 3});
    auto ob = b.start_object(8);
    ob.set_u32(0, static_cast<std::uint32_t>(blob));
    ob.finish_as_root();
    CHECK_HEX(view(b.finish()), "5a41500002000000180000002000000001020300000000001000000000000000");
}

// ── reading a message the P-chain actually shipped ─────────────────────────

void corpus_header() {
    // P_REWARD_VALIDATOR, straight out of conformance/corpus/vectors.tsv.
    const auto bytes = unhex(
        "5a415000020000001000000031000000022a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a"
        "2a2a2a2a");
    auto m = Message::parse(view(bytes));
    CHECK(m.has_value());
    CHECK_EQ(m->version(), kVersion2);
    CHECK_EQ(m->flags(), 0);
    CHECK_EQ(m->size(), 0x31u);
    CHECK_EQ(m->root().offset(), 16);
    CHECK_EQ(m->root().u8(0), 2);  // the tx kind tag
    const auto id = m->root().bytes_fixed(1, 32);
    CHECK_EQ(id.size(), 32u);
    CHECK_EQ(id[0], 0x2a);
    CHECK_EQ(id[31], 0x2a);
    CHECK_EQ(message_length(view(bytes)).value_or(0), 0x31u);
}

// ── what a malformed buffer may not do ─────────────────────────────────────

void refusals() {
    CHECK_EQ(Message::parse({}).error(), Error::BufferTooSmall);
    CHECK_EQ(Message::parse(literal("NOTZAP__________")).error(), Error::InvalidMagic);

    auto bad_version = unhex("5a4150000900000010000000100000000000000000000000");
    CHECK_EQ(Message::parse(view(bad_version)).error(), Error::UnsupportedVersion);

    // A declared size below the header would let root() read off the end.
    auto tiny = unhex("5a4150000200000010000000030000000000000000000000");
    CHECK_EQ(Message::parse(view(tiny)).error(), Error::BufferTooSmall);

    // A declared size beyond the buffer is the same refusal.
    auto over = unhex("5a415000020000001000000099000000");
    CHECK_EQ(Message::parse(view(over)).error(), Error::BufferTooSmall);
}

// A crafted pointer may not reach back into the header, and a lying list
// length may not make a caller loop four billion times.
void pointer_escape() {
    // root at 16, one field: relOffset = -8 (into the header) with length 4.
    Builder b(64);
    auto ob = b.start_object(8);
    ob.finish_as_root();
    auto bytes = b.finish();
    store_u32(bytes.data() + 16, static_cast<std::uint32_t>(-8));
    store_u32(bytes.data() + 20, 4);
    Message m = Message::parse(view(bytes)).value_or(Message{});
    CHECK(m.valid());
    CHECK(m.root().bytes(0).empty());     // an unsigned tail pointer cannot go backwards
    CHECK(m.root().object(0).is_null());  // a signed object pointer cannot reach the header

    // A list whose length word says 0xFFFFFFFF.
    store_u32(bytes.data() + 16, 8);  // forward, in range
    store_u32(bytes.data() + 20, 0xFFFFFFFF);
    m = Message::parse(view(bytes)).value_or(Message{});
    CHECK(m.valid());
    CHECK(m.root().list(0).is_null());
    CHECK(m.root().list_stride(0, 4).is_null());
}

// Reading past the end answers zero rather than faulting: the property that
// lets a hostile buffer reach a typed accessor with no validation pass first.
void reads_are_total() {
    const auto bytes = unhex("5a4150000200000010000000180000000102030405060708");
    auto m = Message::parse(view(bytes));
    CHECK(m.has_value());
    const Object r = m->root();
    CHECK_EQ(r.u64(0), 0x0807060504030201ull);
    CHECK_EQ(r.u64(8), 0u);       // past the end
    CHECK_EQ(r.u32(1 << 20), 0u); // far past the end
    CHECK_EQ(r.u8(-64), 0u);      // before the start
    CHECK(r.bytes_fixed(4, 32).empty());
    CHECK(Object().is_null());
    CHECK_EQ(Object().u32(0), 0u);
    CHECK(List().is_null());
    CHECK_EQ(List().u32(0), 0u);
}

// An out-of-line list: one signed pointer per element, so elements may vary in
// size. It is how a list of transactions is expressed.
void object_ptr_list() {
    Builder b(256);
    auto a = b.start_object(8);
    a.set_u64(0, 11);
    const auto aoff = a.finish();
    auto c = b.start_object(8);
    c.set_u64(0, 22);
    const auto coff = c.finish();

    auto lb = b.start_list(4);
    lb.add_object_ptr(aoff);
    lb.add_object_ptr(coff);
    lb.add_object_ptr(0);
    const auto [off, n] = lb.finish();

    auto root = b.start_object(8);
    root.set_list(0, off, n);
    root.finish_as_root();
    const auto bytes = b.finish();

    auto m = Message::parse(view(bytes));
    CHECK(m.has_value());
    const auto l = m->root().list(0);
    CHECK_EQ(l.size(), 3);
    CHECK_EQ(l.object_ptr(0).u64(0), 11ull);
    CHECK_EQ(l.object_ptr(1).u64(0), 22ull);
    CHECK(l.object_ptr(2).is_null());
    CHECK(l.object_ptr(3).is_null());
    CHECK(l.object_ptr(-1).is_null());
}

// Both wire versions parse; the data segment is identical and only the version
// word differs.
void both_versions_parse() {
    Builder v1(64, kVersion1);
    auto o1 = v1.start_object(8);
    o1.set_u64(0, 42);
    o1.finish_as_root();
    auto a = v1.finish();

    Builder v2(64, kVersion2);
    auto o2 = v2.start_object(8);
    o2.set_u64(0, 42);
    o2.finish_as_root();
    auto c = v2.finish();

    CHECK_EQ(a.size(), c.size());
    CHECK_EQ(std::memcmp(a.data() + 6, c.data() + 6, a.size() - 6), 0);
    CHECK_EQ(Message::parse(view(a))->version(), kVersion1);
    CHECK_EQ(Message::parse(view(c))->version(), kVersion2);
    CHECK_EQ(Message::parse(view(a))->root().u64(0), 42ull);
}

}  // namespace

int main() {
    scalars();
    one_tail();
    two_tails();
    tail_then_list();
    nested();
    text_empty_list();
    write_bytes();
    corpus_header();
    refusals();
    pointer_escape();
    reads_are_total();
    object_ptr_list();
    both_versions_parse();
    return zaptest::verdict("codec");
}
