# ZAP for C++

> Part of the [ZAP Protocol](https://zap-proto.io). The canonical runtime is
> [zap-proto/go](https://github.com/zap-proto/go); this is that protocol in C++.

ZAP is a bidirectional binary protocol with pipelining. Both peers on a
connection initiate, frames are binary, and requests pipeline — many in flight
at once, answers back in whatever order the peer finishes them. It is not a
serialization format with a socket bolted on, and an implementation that treats
it as one has already lost the properties that matter.

Three headers, three layers:

| header | what it is |
| --- | --- |
| `<zap/zap.hpp>` | the message: a 16-byte header, 8-byte-aligned objects and lists, relative pointers. Reads are zero-copy views over a caller-owned buffer, and total — an out-of-range read answers zero rather than faulting. |
| `<zap/rpc.hpp>` | the call: a request/response envelope, and promise pipelining. A dependent call names a prior call's PromiseID as its Target, so the result of A is the input to B without B's caller ever holding it. |
| `<zap/transport.hpp>` | the connection: length-prefixed direction-tagged frames over TCP or a Unix socket. A `Conn` is symmetric — either end may call and either may serve — and `begin()` returns without waiting, which is what puts many requests in flight. |

## Using it

```cmake
find_package(Zap REQUIRED)
target_link_libraries(my_service PRIVATE zap::zap)
```

or, against a tag rather than an install:

```cmake
include(FetchContent)
FetchContent_Declare(Zap
  GIT_REPOSITORY https://github.com/zap-proto/cpp.git
  GIT_TAG        v0.1.1)
FetchContent_MakeAvailable(Zap)
```

A service:

```cpp
#include <zap/transport.hpp>

auto listener = zap::transport::Listener::bind("tcp", "0.0.0.0:9999");
auto conn = listener->accept([](std::span<const std::uint8_t> env)
        -> std::expected<std::vector<std::uint8_t>, std::string> {
    auto call = zap::rpc::parse_request(env);
    if (!call) return std::unexpected(std::string(zap::describe(call.error())));
    return zap::rpc::build_response(zap::rpc::kStatusOK, call->promise_id, answer(*call));
});
```

A caller, with four requests in flight:

```cpp
auto conn = zap::transport::Conn::dial("tcp", "service:9999", nullptr);
zap::rpc::Session session;

std::vector<std::future<zap::transport::Reply>> waiting;
for (int i = 0; i < 4; ++i)
    waiting.push_back((*conn)->begin(zap::rpc::build_request(
        session.origin(session.next(), kMethodGet, {}, key(i)))));
for (auto& w : waiting) use(w.get());
```

Pipelining a dependent call, so B's input never travels back to the caller and
out again:

```cpp
const auto a = session.next();
const auto b = session.next();
conn->begin(zap::rpc::build_request(session.origin(a, kMethodOpen, {}, path)));
conn->begin(zap::rpc::build_request(session.pipeline(b, a, kMethodRead, {}, {})));
```

The server side of that is `zap::rpc::Pipeliner`, which resolves each Target
before dispatch and parks a dependent that arrives ahead of its origin.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

Requires a C++23 compiler (`std::expected`, `std::span`) and POSIX sockets.

## What the tests prove

`test/codec_test.cpp` pins bytes emitted by `zap-proto/go` itself and checks
this runtime writes them exactly — a round-trip test proves an implementation
agrees with itself, and self-agreement is what a fork also has.

`test/transport_test.cpp` proves the two protocol properties without leaning on
a timer. The bidirectional case parks each peer's handler until the other's
request has arrived, so it can only pass if the one connection carries both
directions at once. The pipelining case refuses to answer any request until all
eight have arrived, then answers them in reverse and holds the first back until
the caller — which by then has the last one's answer — says to release it.

## Not here yet

The stream frames (`open` / `msg` / `end`, direction tags 3-5) that the Go
runtime carries. This runtime speaks the unary half of the protocol and refuses
the rest rather than half-answering it.

## License

BSD-3-Clause-Eco. See [LICENSE](LICENSE).
