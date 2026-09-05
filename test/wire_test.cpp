// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// wire_test.cpp — the wire-format suite, case for case out of the canonical
// runtime's own zap_test.go.
//
// It travelled here from a chain that had hand-written its own copy of the
// wire and tested that copy against these cases. The cases were right; the
// second copy of the wire was the problem. They live with the implementation
// now, which is the only place a wire rule can be stated once.
//
// The hostile-buffer cases are not tidiness. A reader that answers a crafted
// buffer with real bytes is a chain that can be told two different things by
// one transaction: an unsigned forward pointer for a byte tail, a floor at the
// header for every pointer, and a length that cannot outrun the buffer it
// names.

#include "check.hpp"

#include <zap/zap.hpp>

#include <cstring>
#include <string>
#include <vector>

using namespace zap;

namespace {

std::span<const std::uint8_t> sp(const std::vector<std::uint8_t>& v) { return {v.data(), v.size()}; }
std::span<const std::uint8_t> str(const char* s) {
    return {reinterpret_cast<const std::uint8_t*>(s), std::strlen(s)};
}
std::string as_string(std::span<const std::uint8_t> b) {
    return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

// The ported cases compare numbers of different widths and signs freely, the
// way their Go originals do; compare them as one wide signed value.
#define CHECK_NUM(want, got) CHECK_EQ(static_cast<long long>(got), static_cast<long long>(want))

void builder() {
    Builder b(256);
    b.write_bytes(str("hello world"));

    auto ob = b.start_object(24);
    ob.set_u32(0, 42);
    ob.set_u64(8, 0xDEADBEEF);
    ob.set_bool(16, true);
    ob.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    const auto root = msg->root();
    CHECK_NUM(42, root.u32(0));
    CHECK_NUM(0xDEADBEEFull, root.u64(8));
    CHECK(root.boolean(16));
}

// Go: TestPrimitives
void primitives() {
    Builder b(256);
    auto ob = b.start_object(64);
    ob.set_u8(0, static_cast<std::uint8_t>(static_cast<std::int8_t>(-42)));
    ob.set_u16(2, static_cast<std::uint16_t>(static_cast<std::int16_t>(-1000)));
    ob.set_u32(4, static_cast<std::uint32_t>(static_cast<std::int32_t>(-100000)));
    ob.set_u64(8, static_cast<std::uint64_t>(static_cast<std::int64_t>(-1000000000)));
    ob.set_u8(16, 255);
    ob.set_u16(18, 65535);
    ob.set_u32(20, 4294967295u);
    ob.set_u64(24, 18446744073709551615ull);
    ob.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    const auto root = msg->root();
    CHECK_NUM(-42, static_cast<std::int8_t>(root.u8(0)));
    CHECK_NUM(-1000, static_cast<std::int16_t>(root.u16(2)));
    CHECK_NUM(-100000, static_cast<std::int32_t>(root.u32(4)));
    CHECK_NUM(-1000000000LL, static_cast<std::int64_t>(root.u64(8)));
    CHECK_NUM(255, root.u8(16));
    CHECK_NUM(65535, root.u16(18));
    CHECK_NUM(4294967295ull, root.u32(20));
    CHECK_NUM(18446744073709551615ull, root.u64(24));
}

// Go: TestList
void list() {
    Builder b(256);
    auto lb = b.start_list(4);
    lb.add_u32(100);
    lb.add_u32(200);
    lb.add_u32(300);

    auto ob = b.start_object(16);
    ob.set_u32(0, 999);
    const auto [lb_off, lb_count] = lb.finish();
    ob.set_list(4, lb_off, lb_count);
    ob.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    const auto root = msg->root();
    CHECK_NUM(999, root.u32(0));

    const auto list = root.list(4);
    CHECK_NUM(3, list.size());
    CHECK_NUM(100, list.u32(0));
    CHECK_NUM(200, list.u32(1));
    CHECK_NUM(300, list.u32(2));
}

// Go: TestByteList
void bytelist() {
    Builder b(256);
    auto lb = b.start_list(1);
    lb.add_bytes(str("hello"));

    auto ob = b.start_object(16);
    const auto [lb_off, lb_count] = lb.finish();
    ob.set_list(0, lb_off, lb_count);
    ob.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    CHECK(as_string(msg->root().list(0).bytes()) == "hello");
}

// Go: TestNestedObject
void nestedobject() {
    Builder b(256);
    auto inner = b.start_object(8);
    inner.set_u32(0, 111);
    inner.set_u32(4, 222);
    const auto inner_off = inner.finish();

    auto outer = b.start_object(16);
    outer.set_u32(0, 333);
    outer.set_object(4, inner_off);
    outer.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    const auto root = msg->root();
    CHECK_NUM(333, root.u32(0));

    const auto obj = root.object(4);
    CHECK(!obj.is_null());
    CHECK_NUM(111, obj.u32(0));
    CHECK_NUM(222, obj.u32(4));
}

// Go: TestTextRoundTrip
void textroundtrip() {
    Builder b(256);
    auto ob = b.start_object(24);
    ob.set_u32(0, 42);
    ob.set_text(4, "Alice");
    ob.set_u32(12, static_cast<std::uint32_t>(30));
    ob.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    const auto root = msg->root();
    CHECK_NUM(42, root.u32(0));
    CHECK(root.text(4) == "Alice");
    CHECK_NUM(30, static_cast<std::int32_t>(root.u32(12)));
}

// Go: TestMultipleTextFields
void multipletextfields() {
    Builder b(256);
    auto ob = b.start_object(24);
    ob.set_text(0, "hello");
    ob.set_text(8, "world");
    ob.set_text(16, "!");
    ob.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    const auto root = msg->root();
    CHECK(root.text(0) == "hello");
    CHECK(root.text(8) == "world");
    CHECK(root.text(16) == "!");
}

// Go: TestNestedObjectWithText
void nestedobjectwithtext() {
    Builder b(512);
    auto inner = b.start_object(16);
    inner.set_text(0, "inner-text");
    inner.set_u32(8, 999);
    const auto inner_off = inner.finish();

    auto outer = b.start_object(16);
    outer.set_text(0, "outer-text");
    outer.set_object(8, inner_off);
    outer.finish_as_root();

    const auto data = b.finish();
    const auto msg = Message::parse(sp(data));
    CHECK(msg.has_value());
    const auto root = msg->root();
    CHECK(root.text(0) == "outer-text");
    const auto obj = root.object(8);
    CHECK(!obj.is_null());
    CHECK(obj.text(0) == "inner-text");
    CHECK_NUM(999, obj.u32(8));
}

// Go: TestInvalidMagic
void invalidmagic() {
    const char* raw = "INVALID_MAGIC___";
    CHECK(!Message::parse(str(raw)).has_value());
}

// Go: TestBufferTooSmall
void buffertoosmall() {
    const std::uint8_t raw[3] = {1, 2, 3};
    CHECK(!Message::parse({raw, 3}).has_value());
}

// Go: TestBytesNegativeRelOffsetRejected — a byte tail's pointer is UNSIGNED, so
// a sign-extended bit pattern becomes a huge forward offset and is refused.
void bytesnegativereloffsetrejected() {
    Builder b(128);
    auto ob = b.start_object(12);
    ob.set_u32(0, 0xDEADBEEF);
    ob.set_bytes(4, str("hello"));
    ob.finish_as_root();
    auto buf = b.finish();

    const std::size_t root_off = load_u32(buf.data() + 8);
    store_u32(buf.data() + root_off + 4, 0xFFFFFFE0u);

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    CHECK(msg->root().bytes(4).empty());
}

// Go: TestBytesMaxUintRelOffsetRejected
void bytesmaxuintreloffsetrejected() {
    Builder b(128);
    auto ob = b.start_object(12);
    ob.set_bytes(4, str("hello"));
    ob.finish_as_root();
    auto buf = b.finish();

    const std::size_t root_off = load_u32(buf.data() + 8);
    store_u32(buf.data() + root_off + 4, 0xFFFFFFFFu);

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    CHECK(msg->root().bytes(4).empty());
}

// Go: TestRedRound2_HIGH1_UncappedListLength
void uncappedlistlength() {
    Builder b(128);
    auto lb = b.start_list(4);
    lb.add_u32(42);
    auto ob = b.start_object(8);
    const auto [lb_off, lb_count] = lb.finish();
    ob.set_list(0, lb_off, lb_count);
    ob.finish_as_root();
    auto buf = b.finish();

    const std::size_t root_off = load_u32(buf.data() + 8);
    store_u32(buf.data() + root_off + 4, 0xFFFFFFFFu);

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    const auto list = msg->root().list(0);
    CHECK(list.size() != static_cast<int>(0xFFFFFFFF));
    CHECK(static_cast<std::size_t>(list.size()) <= buf.size());
}

// Go: TestNewV1_ListStrideTighterClamp
void liststridetighterclamp() {
    Builder b(512);
    auto lb = b.start_list(4);
    for (int i = 0; i < 32; ++i) lb.add_u32(static_cast<std::uint32_t>(i));
    auto ob = b.start_object(8);
    const auto [lb_off, lb_count] = lb.finish();
    ob.set_list(0, lb_off, lb_count);
    ob.finish_as_root();
    auto buf = b.finish();

    const std::size_t root_off = load_u32(buf.data() + 8);
    store_u32(buf.data() + root_off + 4, 100);

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    // The bare accessor cannot know the stride, so it applies only the
    // permissive baseline and accepts.
    CHECK_NUM(100, msg->root().list(0).size());
    // Told the stride, the same buffer is refused.
    CHECK(msg->root().list_stride(0, 4).is_null());
}

// Go: TestNewV1_ListStrideAcceptsHonestLength
void liststrideacceptshonestlength() {
    Builder b(256);
    auto lb = b.start_list(4);
    for (int i = 0; i < 5; ++i) lb.add_u32(static_cast<std::uint32_t>(0xAA00 + i));
    auto ob = b.start_object(8);
    const auto [lb_off, lb_count] = lb.finish();
    ob.set_list(0, lb_off, lb_count);
    ob.finish_as_root();
    const auto buf = b.finish();

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    const auto list = msg->root().list_stride(0, 4);
    CHECK(!list.is_null());
    CHECK_NUM(5, list.size());
    for (int i = 0; i < 5; ++i) CHECK_NUM(0xAA00 + i, list.u32(i));
}

// Go: TestRedRound2_HIGH2_BackwardListPointer
void backwardlistpointer() {
    Builder b(256);
    auto lb = b.start_list(4);
    lb.add_u32(0xAA);
    lb.add_u32(0xBB);
    auto outer = b.start_object(8);
    const auto [lb_off, lb_count] = lb.finish();
    outer.set_list(0, lb_off, lb_count);
    outer.finish_as_root();
    auto buf = b.finish();

    const std::int64_t root_off = load_u32(buf.data() + 8);
    store_u32(buf.data() + root_off, static_cast<std::uint32_t>(static_cast<std::int32_t>(-root_off)));

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    CHECK_NUM(0, msg->root().list(0).size());
}

// Go: TestRedRound2_HIGH2_BackwardObjectPointer
void backwardobjectpointer() {
    Builder b(256);
    auto inner = b.start_object(8);
    inner.set_u32(0, 0xCAFEBABE);
    const auto inner_off = inner.finish();
    auto outer = b.start_object(8);
    outer.set_object(0, inner_off);
    outer.finish_as_root();
    auto buf = b.finish();

    const std::int64_t root_off = load_u32(buf.data() + 8);
    store_u32(buf.data() + root_off, static_cast<std::uint32_t>(static_cast<std::int32_t>(-root_off)));

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    CHECK(msg->root().object(0).is_null());
}

// Go: TestRedRound2_HIGH2_BackwardBytesPointer
void backwardbytespointer() {
    Builder b(128);
    auto ob = b.start_object(12);
    ob.set_bytes(4, str("hello"));
    ob.finish_as_root();
    auto buf = b.finish();

    const std::int64_t root_off = load_u32(buf.data() + 8);
    const auto rel = static_cast<std::uint32_t>(-(root_off + 4));
    store_u32(buf.data() + root_off + 4, rel);
    store_u32(buf.data() + root_off + 8, 4);

    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    CHECK(msg->root().bytes(4).empty());
}

// Go: TestRedRound2_MEDIUM1_VersionParse
void versionparse() {
    auto header = [](std::uint16_t version, std::uint32_t root, std::uint32_t size) {
        std::vector<std::uint8_t> h(kHeaderSize, 0);
        std::memcpy(h.data(), kMagic, 4);
        store_u16(h.data() + 4, version);
        store_u32(h.data() + 8, root);
        store_u32(h.data() + 12, size);
        return h;
    };
    const auto v1 = header(kVersion1, kHeaderSize, kHeaderSize);
    CHECK(Message::parse(sp(v1)).has_value());
    const auto v2 = header(kVersion2, kHeaderSize, kHeaderSize);
    CHECK(Message::parse(sp(v2)).has_value());
    const auto bad = header(99, kHeaderSize, kHeaderSize);
    CHECK(!Message::parse(sp(bad)).has_value());
}

// Go: TestRedRound2_MEDIUM1_NewBuilderEmitsV2
void builderemitsv2() {
    Builder b(128);
    auto ob = b.start_object(8);
    ob.set_u32(0, 42);
    ob.finish_as_root();
    const auto buf = b.finish();
    CHECK_NUM(kVersion2, load_u16(buf.data() + 4));
    const auto msg = Message::parse(sp(buf));
    CHECK(msg.has_value());
    CHECK_NUM(kVersion2, msg->version());
}

// Go: TestRedRound2_V18_SizeZeroRejected
void sizezerorejected() {
    std::vector<std::uint8_t> h(kHeaderSize, 0);
    std::memcpy(h.data(), kMagic, 4);
    store_u16(h.data() + 4, kVersion2);
    store_u32(h.data() + 8, 0);
    store_u32(h.data() + 12, 0);
    CHECK(!Message::parse(sp(h)).has_value());
}

// Go: TestRedRound2_F1_NegativeBitPatternSweep — every high-bit pointer refused.
void negativebitpatternsweep() {
    for (std::uint32_t v = 0xFFFFFFE0u;; ++v) {
        Builder b(128);
        auto ob = b.start_object(12);
        ob.set_u32(0, 0xDEADBEEF);
        ob.set_bytes(4, str("hello"));
        ob.finish_as_root();
        auto buf = b.finish();

        const std::size_t root_off = load_u32(buf.data() + 8);
        store_u32(buf.data() + root_off + 4, v);
        const auto msg = Message::parse(sp(buf));
        CHECK(msg.has_value());
        CHECK(msg->root().bytes(4).empty());
        if (v == 0xFFFFFFFFu) break;
    }
}

// The self-delimiting length: the split point between a signed transaction's
// unsigned prefix and its credential suffix.
void messagelengthisthesplitpoint() {
    Builder b(128);
    auto ob = b.start_object(8);
    ob.set_u64(0, 7);
    ob.finish_as_root();
    auto buf = b.finish();
    const auto n = message_length(sp(buf));
    CHECK(n.has_value());
    CHECK_NUM(buf.size(), *n);

    // Append a tail; the length still names only the leading message.
    buf.push_back(0xAB);
    const auto n2 = message_length(sp(buf));
    CHECK(n2.has_value());
    CHECK_NUM(buf.size() - 1, *n2);
}

}  // namespace

int main() {
    builder();
    primitives();
    list();
    bytelist();
    nestedobject();
    textroundtrip();
    multipletextfields();
    nestedobjectwithtext();
    invalidmagic();
    buffertoosmall();
    bytesnegativereloffsetrejected();
    bytesmaxuintreloffsetrejected();
    uncappedlistlength();
    liststridetighterclamp();
    liststrideacceptshonestlength();
    backwardlistpointer();
    backwardobjectpointer();
    backwardbytespointer();
    versionparse();
    builderemitsv2();
    sizezerorejected();
    negativebitpatternsweep();
    messagelengthisthesplitpoint();
    return zaptest::verdict("wire");
}
