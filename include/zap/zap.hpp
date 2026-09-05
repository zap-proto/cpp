// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// zap.hpp — the ZAP message layer for C++.
//
// ZAP is a bidirectional binary protocol. This header is the part of it that
// says what a message IS; rpc.hpp says what a call is, and transport.hpp
// carries them over a connection where BOTH peers initiate and requests
// pipeline. A program that includes only this header has the vocabulary, not
// the protocol.
//
// The layout is rendered field for field from the canonical runtime,
// github.com/zap-proto/go (zap.go, builder.go). Not "compatible with": the
// same arithmetic over the same layout, so a buffer written here parses there
// and a buffer written there parses here. Two encoders that agree by
// convention are two wire formats waiting to diverge.
//
//   Header (16 bytes)
//     magic   "ZAP\0"    4 @ 0
//     version u16 LE     2 @ 4    1 = original layout, 2 = leading tag byte
//     flags   u16 LE     2 @ 6    high byte carries the transport msgType
//     root    u32 LE     4 @ 8    absolute offset of the root object
//     size    u32 LE     4 @ 12   total length, header included
//
// After the header comes the data segment: 8-byte-aligned object payloads,
// list payloads and byte tails. Every multi-byte integer is little endian, and
// every pointer field holds an offset RELATIVE to the pointer's own position,
// which is what lets a whole message move without being rewritten.
//
// Reading is total. An out-of-range read answers zero rather than faulting, so
// a hostile buffer can be handed straight to a typed accessor with no
// validation pass in front of it: bounds live here, once, instead of in every
// caller. Callers still decide MEANING; this layer only promises that a
// malformed buffer cannot reach past its end.
//
// The read side is a VIEW. Message, Object and List alias a caller-owned
// buffer and copy nothing, so the buffer must outlive them. That is the point
// of the format.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace zap {

inline constexpr std::size_t kHeaderSize = 16;
inline constexpr std::size_t kAlignment = 8;
inline constexpr std::uint16_t kVersion1 = 1;
inline constexpr std::uint16_t kVersion2 = 2;

// kVersion is the header this runtime writes. Version 2 is what the transport
// envelope and every chain on this wire emit; both versions parse, and the data
// segment is byte-identical between them — the version word is the only
// difference between a v1 and a v2 buffer carrying the same payload.
inline constexpr std::uint16_t kVersion = kVersion2;

inline constexpr char kMagic[4] = {'Z', 'A', 'P', '\0'};

// Header flags. The high byte of the flags word is the transport msgType (see
// rpc.hpp), so a flag here occupies the low byte.
inline constexpr std::uint16_t kFlagNone = 0;
inline constexpr std::uint16_t kFlagCompressed = 1u << 0;
inline constexpr std::uint16_t kFlagEncrypted = 1u << 1;
inline constexpr std::uint16_t kFlagSigned = 1u << 2;

// Why a buffer is not a message. The text is the canonical runtime's own.
enum class Error {
    BufferTooSmall,
    InvalidMagic,
    UnsupportedVersion,
};

constexpr std::string_view describe(Error e) {
    switch (e) {
        case Error::InvalidMagic:
            return "zap: invalid magic bytes";
        case Error::UnsupportedVersion:
            return "zap: unsupported version";
        case Error::BufferTooSmall:
            break;
    }
    return "zap: buffer too small";
}

// ── little-endian primitives; the ONE place byte order is spelled ───────────

inline std::uint16_t load_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | static_cast<std::uint16_t>(p[1] << 8);
}
inline std::uint32_t load_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
inline std::uint64_t load_u64(const std::uint8_t* p) {
    return static_cast<std::uint64_t>(load_u32(p)) |
           (static_cast<std::uint64_t>(load_u32(p + 4)) << 32);
}
inline void store_u16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
inline void store_u32(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
    p[2] = static_cast<std::uint8_t>(v >> 16);
    p[3] = static_cast<std::uint8_t>(v >> 24);
}
inline void store_u64(std::uint8_t* p, std::uint64_t v) {
    store_u32(p, static_cast<std::uint32_t>(v));
    store_u32(p + 4, static_cast<std::uint32_t>(v >> 32));
}

// message_length is the self-delimiting length of the leading message in a
// buffer: the size word at offset 12. It is what splits a signed payload from
// the credential suffix that follows it, and what a stream reader uses to find
// the end of one message without parsing it.
inline std::optional<std::size_t> message_length(std::span<const std::uint8_t> b) {
    if (b.size() < kHeaderSize) return std::nullopt;
    const std::size_t n = load_u32(b.data() + 12);
    if (n < kHeaderSize || n > b.size()) return std::nullopt;
    return n;
}

class Object;

// List is a view of a run of same-shaped elements. The stride is NOT on the
// wire — it belongs to the schema, so the accessor supplies it.
class List {
  public:
    List() = default;
    List(const std::uint8_t* data, std::size_t size, std::int64_t offset, std::int64_t length)
        : d_(data), n_(size), off_(offset), len_(length) {}

    std::int64_t size() const { return len_; }
    bool is_null() const { return d_ == nullptr; }

    std::uint8_t u8(std::int64_t i) const {
        if (i < 0 || i >= len_) return 0;
        const std::int64_t pos = off_ + i;
        if (static_cast<std::uint64_t>(pos) >= n_) return 0;
        return d_[pos];
    }
    std::uint32_t u32(std::int64_t i) const {
        if (i < 0 || i >= len_) return 0;
        const std::int64_t pos = off_ + i * 4;
        if (static_cast<std::uint64_t>(pos) + 4 > n_) return 0;
        return load_u32(d_ + pos);
    }
    std::uint64_t u64(std::int64_t i) const {
        if (i < 0 || i >= len_) return 0;
        const std::int64_t pos = off_ + i * 8;
        if (static_cast<std::uint64_t>(pos) + 8 > n_) return 0;
        return load_u64(d_ + pos);
    }

    // object(i, stride) is element i of an INLINE list: fixed-stride records
    // laid down back to back.
    Object object(std::int64_t i, std::int64_t stride) const;

    // object_ptr(i) is element i of an OUT-OF-LINE list: a 4-byte signed
    // relative pointer per element, dereferenced exactly as Object::object
    // does. It is how a list of variably-sized objects is expressed.
    Object object_ptr(std::int64_t i) const;

    std::span<const std::uint8_t> bytes() const {
        if (d_ == nullptr || static_cast<std::uint64_t>(off_ + len_) > n_) return {};
        return {d_ + off_, static_cast<std::size_t>(len_)};
    }

  private:
    const std::uint8_t* d_ = nullptr;
    std::size_t n_ = 0;
    std::int64_t off_ = 0;
    std::int64_t len_ = 0;
};

// Object is a view of one struct payload: a base offset plus the schema's
// field offsets. It carries the buffer with it rather than a back-pointer to
// the Message, so an Object outliving a local Message is still safe to read.
class Object {
  public:
    Object() = default;
    Object(const std::uint8_t* data, std::size_t size, std::int64_t offset)
        : d_(data), n_(size), off_(offset) {}

    bool is_null() const { return d_ == nullptr || off_ == 0; }
    std::int64_t offset() const { return off_; }
    const std::uint8_t* buffer() const { return d_; }
    std::size_t buffer_size() const { return n_; }

    bool boolean(std::int64_t field) const { return u8(field) != 0; }

    std::uint8_t u8(std::int64_t field) const {
        const std::int64_t pos = off_ + field;
        if (d_ == nullptr || pos < 0 || static_cast<std::uint64_t>(pos) >= n_) return 0;
        return d_[pos];
    }
    std::uint16_t u16(std::int64_t field) const {
        const std::int64_t pos = off_ + field;
        if (d_ == nullptr || pos < 0 || static_cast<std::uint64_t>(pos) + 2 > n_) return 0;
        return load_u16(d_ + pos);
    }
    std::uint32_t u32(std::int64_t field) const {
        const std::int64_t pos = off_ + field;
        if (d_ == nullptr || pos < 0 || static_cast<std::uint64_t>(pos) + 4 > n_) return 0;
        return load_u32(d_ + pos);
    }
    std::uint64_t u64(std::int64_t field) const {
        const std::int64_t pos = off_ + field;
        if (d_ == nullptr || pos < 0 || static_cast<std::uint64_t>(pos) + 8 > n_) return 0;
        return load_u64(d_ + pos);
    }

    // A fixed-width byte run living IN the payload — an id, a public key — as
    // opposed to bytes(), which follows a pointer to a variable tail.
    std::span<const std::uint8_t> bytes_fixed(std::int64_t field, std::int64_t len) const {
        if (d_ == nullptr || len <= 0) return {};
        const std::int64_t pos = off_ + field;
        if (pos < 0 || static_cast<std::uint64_t>(pos + len) > n_) return {};
        return {d_ + pos, static_cast<std::size_t>(len)};
    }

    // A variable byte tail: {relOffset u32, length u32}. relOffset is an
    // UNSIGNED forward pointer, so a crafted high-bit value becomes a huge
    // positive and dies on the end bound instead of aliasing backwards; a tail
    // landing inside the wire header is refused for the same reason. That pair
    // of rules is what closes the pointer-escape surface.
    std::span<const std::uint8_t> bytes(std::int64_t field) const {
        const std::int64_t pos = off_ + field;
        if (d_ == nullptr || pos < 0 || static_cast<std::uint64_t>(pos) + 4 > n_) return {};
        const std::uint32_t rel = load_u32(d_ + pos);
        if (rel == 0) return {};
        const std::int64_t len_pos = pos + 4;
        if (static_cast<std::uint64_t>(len_pos) + 4 > n_) return {};
        const std::uint32_t len = load_u32(d_ + len_pos);
        const std::int64_t abs = pos + static_cast<std::int64_t>(rel);
        if (abs < static_cast<std::int64_t>(kHeaderSize)) return {};
        if (static_cast<std::uint64_t>(abs) + len > n_) return {};
        return {d_ + abs, static_cast<std::size_t>(len)};
    }

    std::string_view text(std::int64_t field) const {
        const auto b = bytes(field);
        if (b.empty()) return {};
        return {reinterpret_cast<const char*>(b.data()), b.size()};
    }

    // A nested object pointer. relOffset is SIGNED here: a builder may finalize
    // a child before its parent, in which case the child lives earlier in the
    // segment and the pointer runs backwards. A target inside the wire header
    // is still refused — the header is Magic/Version/Flags/Root/Size and never
    // a legitimate payload.
    Object object(std::int64_t field) const {
        const std::int64_t pos = off_ + field;
        if (d_ == nullptr || pos < 0 || static_cast<std::uint64_t>(pos) + 4 > n_) return {};
        const std::int32_t rel = static_cast<std::int32_t>(load_u32(d_ + pos));
        if (rel == 0) return {};
        const std::int64_t abs = pos + rel;
        if (abs < static_cast<std::int64_t>(kHeaderSize) || static_cast<std::uint64_t>(abs) >= n_)
            return {};
        return Object(d_, n_, abs);
    }

    // A list pointer: {relOffset i32, length u32}. The length is clamped to the
    // buffer so an attacker-set 0xFFFFFFFF cannot make a caller loop four
    // billion times even though every element accessor answers zero.
    List list(std::int64_t field) const { return list_stride(field, 0); }

    // list_stride is list() with the tighter clamp the schema makes possible:
    // length * stride must fit in what remains of the buffer, so a lying count
    // is refused once, up front, rather than at every element.
    List list_stride(std::int64_t field, std::uint32_t stride) const {
        const std::int64_t pos = off_ + field;
        if (d_ == nullptr || pos < 0 || static_cast<std::uint64_t>(pos) + 8 > n_) return {};
        const std::int32_t rel = static_cast<std::int32_t>(load_u32(d_ + pos));
        if (rel == 0) return {};
        const std::uint32_t len = load_u32(d_ + pos + 4);
        const std::int64_t abs = pos + rel;
        if (abs < static_cast<std::int64_t>(kHeaderSize) || static_cast<std::uint64_t>(abs) >= n_)
            return {};
        if (stride > 0) {
            const std::uint64_t remaining = n_ - static_cast<std::uint64_t>(abs);
            if (static_cast<std::uint64_t>(len) * stride > remaining) return {};
        } else if (static_cast<std::uint64_t>(len) > n_) {
            return {};
        }
        return List(d_, n_, abs, len);
    }

  private:
    const std::uint8_t* d_ = nullptr;
    std::size_t n_ = 0;
    std::int64_t off_ = 0;
};

inline Object List::object(std::int64_t i, std::int64_t stride) const {
    if (i < 0 || i >= len_) return {};
    return Object(d_, n_, off_ + i * stride);
}

inline Object List::object_ptr(std::int64_t i) const {
    if (i < 0 || i >= len_) return {};
    const std::int64_t pos = off_ + i * 4;
    if (static_cast<std::uint64_t>(pos) + 4 > n_) return {};
    const std::int32_t rel = static_cast<std::int32_t>(load_u32(d_ + pos));
    if (rel == 0) return {};
    const std::int64_t abs = pos + rel;
    if (abs < static_cast<std::int64_t>(kHeaderSize) || static_cast<std::uint64_t>(abs) >= n_)
        return {};
    return Object(d_, n_, abs);
}

// Message is a validated window onto caller-owned bytes. It owns nothing.
class Message {
  public:
    Message() = default;

    // parse checks magic, version and the declared size, and says which one
    // failed rather than handing back a half-valid message. A size word below
    // the header would let Root()/Flags() read off an empty slice, so it is
    // refused at the boundary.
    static std::expected<Message, Error> parse(std::span<const std::uint8_t> data) {
        if (data.size() < kHeaderSize) return std::unexpected(Error::BufferTooSmall);
        if (std::memcmp(data.data(), kMagic, 4) != 0) return std::unexpected(Error::InvalidMagic);
        const std::uint16_t version = load_u16(data.data() + 4);
        if (version != kVersion1 && version != kVersion2)
            return std::unexpected(Error::UnsupportedVersion);
        const std::uint32_t size = load_u32(data.data() + 12);
        if (size < kHeaderSize || static_cast<std::size_t>(size) > data.size())
            return std::unexpected(Error::BufferTooSmall);
        return Message(data.data(), size);
    }

    bool valid() const { return d_ != nullptr; }
    std::span<const std::uint8_t> bytes() const { return {d_, n_}; }
    std::size_t size() const { return n_; }
    std::uint16_t version() const { return d_ ? load_u16(d_ + 4) : 0; }
    std::uint16_t flags() const { return d_ ? load_u16(d_ + 6) : 0; }

    Object root() const {
        if (d_ == nullptr) return {};
        return Object(d_, n_, static_cast<std::int64_t>(load_u32(d_ + 8)));
    }

  private:
    Message(const std::uint8_t* d, std::size_t n) : d_(d), n_(n) {}
    const std::uint8_t* d_ = nullptr;
    std::size_t n_ = 0;
};

class ObjectBuilder;
class ListBuilder;

// Builder writes a message. It hands out ObjectBuilders and ListBuilders that
// append into its one buffer; a builder is single-threaded by construction,
// because a message has exactly one cursor.
class Builder {
  public:
    explicit Builder(std::size_t capacity = 256, std::uint16_t version = kVersion) {
        if (capacity < kHeaderSize) capacity = 256;
        buf_.assign(capacity, 0);
        pos_ = static_cast<std::int64_t>(kHeaderSize);
        std::memcpy(buf_.data(), kMagic, 4);
        store_u16(buf_.data() + 4, version);
    }

    void reset() {
        pos_ = static_cast<std::int64_t>(kHeaderSize);
        root_ = 0;
    }

    ObjectBuilder start_object(std::int64_t data_size);
    ListBuilder start_list(std::int64_t stride = 0);

    std::int64_t write_bytes(std::span<const std::uint8_t> data) {
        if (data.empty()) return 0;
        align(kAlignment);
        const std::int64_t off = pos_;
        grow(data.size());
        std::memcpy(buf_.data() + pos_, data.data(), data.size());
        pos_ += static_cast<std::int64_t>(data.size());
        return off;
    }

    std::int64_t write_text(std::string_view s) {
        return write_bytes({reinterpret_cast<const std::uint8_t*>(s.data()), s.size()});
    }

    std::vector<std::uint8_t> finish() {
        store_u32(buf_.data() + 8, static_cast<std::uint32_t>(root_));
        store_u32(buf_.data() + 12, static_cast<std::uint32_t>(pos_));
        return std::vector<std::uint8_t>(buf_.begin(), buf_.begin() + pos_);
    }

    std::vector<std::uint8_t> finish_with_flags(std::uint16_t flags) {
        store_u16(buf_.data() + 6, flags);
        return finish();
    }

  private:
    friend class ObjectBuilder;
    friend class ListBuilder;

    void grow(std::size_t n) {
        if (static_cast<std::size_t>(pos_) + n <= buf_.size()) return;
        std::size_t cap = buf_.size() * 2;
        if (cap < static_cast<std::size_t>(pos_) + n) cap = static_cast<std::size_t>(pos_) + n;
        buf_.resize(cap, 0);
    }

    void align(std::size_t alignment) {
        const std::size_t pad =
            (alignment - (static_cast<std::size_t>(pos_) % alignment)) % alignment;
        grow(pad);
        for (std::size_t i = 0; i < pad; ++i) buf_[static_cast<std::size_t>(pos_++)] = 0;
    }

    std::vector<std::uint8_t> buf_;
    std::int64_t pos_ = 0;
    std::int64_t root_ = 0;
};

// ObjectBuilder writes one object's fixed payload and the tails its pointer
// fields name.
//
// start_object reserves the whole fixed payload up front and zero-fills it, so
// fields may be set in any order and a tail appended afterwards can never land
// on top of a field not written yet. A tail set by set_bytes is held until
// finish() and appended then, in the order the calls were made — the canonical
// runtime's rule, and the reason a nested object started between two set_bytes
// calls lands where the reference puts it rather than where C++ evaluation
// order happens to.
class ObjectBuilder {
  public:
    ObjectBuilder() = default;
    ObjectBuilder(Builder* b, std::int64_t start, std::int64_t data_size)
        : b_(b), start_(start), data_size_(data_size) {}

    void set_bool(std::int64_t field, bool v) { set_u8(field, v ? 1 : 0); }
    void set_u8(std::int64_t field, std::uint8_t v) {
        ensure_field(field + 1);
        b_->buf_[static_cast<std::size_t>(start_ + field)] = v;
    }
    void set_u16(std::int64_t field, std::uint16_t v) {
        ensure_field(field + 2);
        store_u16(b_->buf_.data() + start_ + field, v);
    }
    void set_u32(std::int64_t field, std::uint32_t v) {
        ensure_field(field + 4);
        store_u32(b_->buf_.data() + start_ + field, v);
    }
    void set_u64(std::int64_t field, std::uint64_t v) {
        ensure_field(field + 8);
        store_u64(b_->buf_.data() + start_ + field, v);
    }

    // set_bytes_fixed copies IN PLACE inside the fixed payload — a declared
    // fixed-width field, not a pointer. Empty is a no-op, as in the reference.
    void set_bytes_fixed(std::int64_t field, std::span<const std::uint8_t> v) {
        if (v.empty()) return;
        ensure_field(field + static_cast<std::int64_t>(v.size()));
        std::memcpy(b_->buf_.data() + start_ + field, v.data(), v.size());
    }

    // set_bytes writes {relOffset u32, length u32} now and the payload at
    // finish(). Empty is a null pointer pair.
    void set_bytes(std::int64_t field, std::span<const std::uint8_t> v) {
        ensure_field(field + 8);
        if (v.empty()) {
            store_u32(b_->buf_.data() + start_ + field, 0);
            store_u32(b_->buf_.data() + start_ + field + 4, 0);
            return;
        }
        store_u32(b_->buf_.data() + start_ + field + 4, static_cast<std::uint32_t>(v.size()));
        tails_.push_back(Tail{field, std::vector<std::uint8_t>(v.begin(), v.end())});
    }

    void set_text(std::int64_t field, std::string_view v) {
        set_bytes(field, {reinterpret_cast<const std::uint8_t*>(v.data()), v.size()});
    }

    void set_object(std::int64_t field, std::int64_t obj_offset) {
        ensure_field(field + 4);
        if (obj_offset == 0) {
            store_u32(b_->buf_.data() + start_ + field, 0);
            return;
        }
        const std::int64_t rel = obj_offset - (start_ + field);
        store_u32(b_->buf_.data() + start_ + field,
                  static_cast<std::uint32_t>(static_cast<std::int32_t>(rel)));
    }

    void set_list(std::int64_t field, std::int64_t list_offset, std::int64_t length) {
        ensure_field(field + 8);
        if (list_offset == 0 || length == 0) {
            store_u32(b_->buf_.data() + start_ + field, 0);
            store_u32(b_->buf_.data() + start_ + field + 4, 0);
            return;
        }
        const std::int64_t rel = list_offset - (start_ + field);
        store_u32(b_->buf_.data() + start_ + field,
                  static_cast<std::uint32_t>(static_cast<std::int32_t>(rel)));
        store_u32(b_->buf_.data() + start_ + field + 4, static_cast<std::uint32_t>(length));
    }

    // finish appends every held tail after the fixed payload, patches each
    // pointer, and returns the object's offset.
    std::int64_t finish() {
        ensure_field(data_size_);
        for (const auto& t : tails_) {
            const std::int64_t data_pos = b_->pos_;
            b_->grow(t.data.size());
            std::memcpy(b_->buf_.data() + b_->pos_, t.data.data(), t.data.size());
            b_->pos_ += static_cast<std::int64_t>(t.data.size());
            const std::int64_t field_abs = start_ + t.field;
            store_u32(b_->buf_.data() + field_abs,
                      static_cast<std::uint32_t>(static_cast<std::int32_t>(data_pos - field_abs)));
        }
        tails_.clear();
        return start_;
    }

    std::int64_t finish_as_root() {
        const std::int64_t off = finish();
        b_->root_ = off;
        return off;
    }

  private:
    friend class Builder;

    struct Tail {
        std::int64_t field;
        std::vector<std::uint8_t> data;
    };

    void ensure_field(std::int64_t end_offset) {
        const std::int64_t needed = start_ + end_offset;
        if (needed <= b_->pos_) return;
        b_->grow(static_cast<std::size_t>(needed - b_->pos_));
        for (std::int64_t i = b_->pos_; i < needed; ++i) b_->buf_[static_cast<std::size_t>(i)] = 0;
        b_->pos_ = needed;
    }

    Builder* b_ = nullptr;
    std::int64_t start_ = 0;
    std::int64_t data_size_ = 0;
    std::vector<Tail> tails_;
};

// ListBuilder appends elements at the builder cursor. The stride is implicit in
// which add_ the caller uses.
//
// add_bytes counts BYTES, not elements — the reference's rule, and the reason a
// caller writing fixed-stride records passes the real element count to set_list
// itself. Changing it here would silently change the length word on the wire.
class ListBuilder {
  public:
    ListBuilder() = default;
    ListBuilder(Builder* b, std::int64_t start) : b_(b), start_(start) {}

    void add_u8(std::uint8_t v) {
        b_->grow(1);
        b_->buf_[static_cast<std::size_t>(b_->pos_++)] = v;
        ++count_;
    }
    void add_u32(std::uint32_t v) {
        b_->grow(4);
        store_u32(b_->buf_.data() + b_->pos_, v);
        b_->pos_ += 4;
        ++count_;
    }
    void add_u64(std::uint64_t v) {
        b_->grow(8);
        store_u64(b_->buf_.data() + b_->pos_, v);
        b_->pos_ += 8;
        ++count_;
    }
    void add_bytes(std::span<const std::uint8_t> data) {
        b_->grow(data.size());
        if (!data.empty()) std::memcpy(b_->buf_.data() + b_->pos_, data.data(), data.size());
        b_->pos_ += static_cast<std::int64_t>(data.size());
        count_ += static_cast<std::int64_t>(data.size());
    }
    // add_object_ptr appends one 4-byte SIGNED relative pointer to an object
    // already written at target_pos (0 = a null element). It is the write side
    // of List::object_ptr.
    void add_object_ptr(std::int64_t target_pos) {
        b_->grow(4);
        if (target_pos == 0) {
            store_u32(b_->buf_.data() + b_->pos_, 0);
        } else {
            const std::int64_t rel = target_pos - b_->pos_;
            store_u32(b_->buf_.data() + b_->pos_,
                      static_cast<std::uint32_t>(static_cast<std::int32_t>(rel)));
        }
        b_->pos_ += 4;
        ++count_;
    }

    // finish returns (offset, count) — the pair set_list takes.
    std::pair<std::int64_t, std::int64_t> finish() const { return {start_, count_}; }

  private:
    Builder* b_ = nullptr;
    std::int64_t start_ = 0;
    std::int64_t count_ = 0;
};

inline ObjectBuilder Builder::start_object(std::int64_t data_size) {
    align(kAlignment);
    ObjectBuilder ob(this, pos_, data_size);
    ob.ensure_field(data_size);
    return ob;
}

inline ListBuilder Builder::start_list(std::int64_t /*stride*/) {
    align(kAlignment);
    return ListBuilder(this, pos_);
}

}  // namespace zap
