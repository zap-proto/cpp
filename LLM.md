# zap-proto/cpp

The ZAP protocol in C++. Three layers, three headers, one library target
`zap::zap`.

## What ZAP is, and what it is not

ZAP is a **bidirectional binary protocol with pipelining**. Both peers on a
connection initiate; requests pipeline, many in flight, answers back out of
order. It is *not* a codec and not a serialization format. Framing a struct and
calling that ZAP loses exactly the properties that matter.

The typed schema is the source. JSON, MCP, docs, OpenAPI, CLI and SDKs are
generated *from* it. Nothing here parses JSON into ZAP, and no service
hand-rolls a frame writer.

## Layers

- `include/zap/zap.hpp` — the message. 16-byte header (`"ZAP\0"`, version u16,
  flags u16, root u32, size u32), then 8-byte-aligned object payloads, list
  payloads and byte tails. Every pointer is relative to its own position.
  Little endian throughout. `Message`/`Object`/`List` are views over
  caller-owned bytes and copy nothing; the buffer must outlive them.
- `include/zap/rpc.hpp`, `src/rpc.cpp` — the call envelope (request fixed size
  28, response fixed size 20, flags `kMsgTypeRouterBase << 8`), plus `Session`
  (client-side PromiseID allocation and Target stamping) and `Pipeliner`
  (server-side promise table).
- `include/zap/transport.hpp`, `src/transport.cpp` — frames
  `[u32 len][u8 dir][envelope]` over TCP or Unix. `Conn` is symmetric and
  `begin()` does not wait.

## Rules that are not negotiable

- **The canonical runtime is `github.com/zap-proto/go`.** Any layout question
  is settled by reading `zap.go` / `builder.go` / `rpc/envelope.go`, not by
  reasoning about what would be nicer. The KATs in `test/codec_test.cpp` and
  `test/rpc_test.cpp` are bytes that runtime printed.
- **`ObjectBuilder::set_bytes` holds its payload until `finish()`.** Writing it
  on the spot produces different bytes whenever a list or nested object is
  started on the same builder between the `set_bytes` and the `finish`. The
  reference defers; so does this. `tail_then_list` in the codec test is that
  exact case, and it is why the rule is written down here.
- **`ListBuilder::add_bytes` counts BYTES, not elements.** A caller writing
  fixed-stride records passes the real element count to `set_list` itself.
  "Fixing" it silently changes the length word on the wire.
- **Reads are total.** An out-of-range read answers zero. A pointer target
  inside the wire header is refused. A `bytes` relOffset is unsigned (forward
  only); an `object` / `list` relOffset is signed (a child may be finalized
  before its parent). Those four rules are the whole safety story; do not
  scatter bounds checks into callers.
- **This runtime writes version 2** (`kVersion`). `zap-proto/go`'s plain
  `NewBuilder` writes version 1 and its `NewBuilderV2` writes 2; both parse
  either, and the data segment is identical.

## Additions over the Go runtime

These exist here because a schema needs them and the Go runtime expresses them
by hand at the call site. They should be backported there, and to the Rust
runtime, rather than re-invented per consumer:

- `Object::bytes_fixed(field, len)` — a fixed-width byte run living *in* the
  payload (an id, a public key), as opposed to a pointer to a tail.
- `Object::list_stride(field, stride)` — `list()` with the tighter clamp the
  schema makes possible: `length * stride` must fit the remaining buffer.
- `List::object_ptr(i)` / `ListBuilder::add_object_ptr(pos)` — an out-of-line
  list: one signed relative pointer per element, so elements may vary in size.
- `message_length(buffer)` — the self-delimiting length of the leading message,
  which is what splits a signed payload from a credential suffix.

## Not implemented

The stream frames the Go runtime carries (direction tags 3-5: open / msg /
end). An unknown direction tag ends the connection rather than being ignored.

## Testing

`ctest --test-dir build`. Three suites: `codec`, `rpc`, `transport`.

The transport suite proves the two protocol properties without a timer, because
a timing-based proof of "in flight at once" is not a proof. Both peers'
handlers park until the other's request arrives (bidirectional); no request is
answered until all eight have arrived and they are then answered in reverse,
with the first held until the caller releases it (pipelining, out of order).

Run it under ThreadSanitizer before touching `transport.cpp`.
