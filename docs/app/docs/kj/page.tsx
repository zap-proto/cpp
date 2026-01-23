import { DocsLayout } from '@/components/docs-layout'

export default function KJ() {
  return (
    <DocsLayout>
      <h1>KJ Library</h1>

      <p>
        KJ is a modern C++ utility library included with ZAP C++. It provides
        async I/O, string utilities, containers, and more.
      </p>

      <h2>Headers</h2>

      <pre><code>{`#include <kj/common.h>       // Core utilities
#include <kj/string.h>       // String types
#include <kj/array.h>        // Array and ArrayPtr
#include <kj/vector.h>       // Dynamic arrays
#include <kj/memory.h>       // Own, Maybe
#include <kj/async.h>        // Promises, event loop
#include <kj/async-io.h>     // Async networking
#include <kj/exception.h>    // Exception handling`}</code></pre>

      <h2>Strings</h2>

      <h3>StringPtr</h3>

      <p>Non-owning reference to a string:</p>

      <pre><code>{`#include <kj/string.h>

void process(kj::StringPtr str) {
  // Access characters
  for (char c : str) { ... }

  // Get C string
  const char* cstr = str.cStr();

  // Get size
  size_t len = str.size();

  // Comparison
  if (str == "hello") { ... }
  if (str.startsWith("prefix")) { ... }
}`}</code></pre>

      <h3>String</h3>

      <p>Owning string type:</p>

      <pre><code>{`#include <kj/string.h>

// Create string
kj::String str = kj::str("Hello, ", name, "!");

// From heap allocated
kj::String owned = kj::heapString("hello");

// Convert to StringPtr
kj::StringPtr ptr = str;

// Convert to std::string
std::string stdStr = str.cStr();`}</code></pre>

      <h2>Arrays</h2>

      <h3>ArrayPtr</h3>

      <pre><code>{`#include <kj/array.h>

void process(kj::ArrayPtr<const int> arr) {
  for (int x : arr) { ... }

  // Access elements
  int first = arr[0];

  // Slicing
  auto slice = arr.slice(1, 5);
}`}</code></pre>

      <h3>Array</h3>

      <pre><code>{`#include <kj/array.h>

// Create array
kj::Array<int> arr = kj::heapArray<int>(100);

// Initialize
for (size_t i = 0; i < arr.size(); i++) {
  arr[i] = i;
}

// Move semantics
kj::Array<int> arr2 = kj::mv(arr);`}</code></pre>

      <h3>Vector</h3>

      <pre><code>{`#include <kj/vector.h>

kj::Vector<int> vec;
vec.add(1);
vec.add(2);
vec.add(3);

// Convert to Array
kj::Array<int> arr = vec.releaseAsArray();`}</code></pre>

      <h2>Memory Management</h2>

      <h3>Own</h3>

      <p>Unique ownership pointer (like std::unique_ptr):</p>

      <pre><code>{`#include <kj/memory.h>

// Create owned object
kj::Own<MyClass> obj = kj::heap<MyClass>(args...);

// Access
obj->method();

// Move
kj::Own<MyClass> obj2 = kj::mv(obj);

// Release raw pointer
MyClass* raw = obj2.release();`}</code></pre>

      <h3>Maybe</h3>

      <p>Optional value type:</p>

      <pre><code>{`#include <kj/memory.h>

kj::Maybe<int> maybeValue;

// Check if has value
if (maybeValue != kj::none) { ... }

// Pattern matching
KJ_IF_MAYBE(value, maybeValue) {
  // *value is the int
  process(*value);
} else {
  // No value present
}

// Get with default
int value = maybeValue.orDefault(42);`}</code></pre>

      <h2>Async I/O</h2>

      <h3>Event Loop</h3>

      <pre><code>{`#include <kj/async-io.h>

int main() {
  // Set up async I/O context
  kj::AsyncIoContext ctx = kj::setupAsyncIo();
  kj::WaitScope& waitScope = ctx.waitScope;

  // Run async code
  auto promise = doAsyncWork();
  auto result = promise.wait(waitScope);

  return 0;
}`}</code></pre>

      <h3>Promises</h3>

      <pre><code>{`#include <kj/async.h>

// Create a promise that resolves immediately
kj::Promise<int> promise = kj::Promise<int>(42);

// Chain with then()
auto doubled = promise.then([](int x) {
  return x * 2;
});

// Chain multiple operations
auto result = fetchData()
    .then([](Data data) { return process(data); })
    .then([](Result r) { return format(r); });

// Handle errors
auto safe = riskyOperation()
    .catch_([](kj::Exception&& e) {
      return kj::str("Error: ", e.getDescription());
    });`}</code></pre>

      <h3>Forking Promises</h3>

      <pre><code>{`// Fork a promise to use result multiple times
auto fork = promise.fork();

auto branch1 = fork.addBranch().then([](int x) { return x + 1; });
auto branch2 = fork.addBranch().then([](int x) { return x * 2; });`}</code></pre>

      <h3>Joining Promises</h3>

      <pre><code>{`// Wait for multiple promises
auto joined = kj::joinPromises(kj::mv(promises));

// Join specific promises
auto both = promise1.then([&](int a) {
  return promise2.then([a](int b) {
    return a + b;
  });
});`}</code></pre>

      <h2>Networking</h2>

      <h3>TCP Server</h3>

      <pre><code>{`#include <kj/async-io.h>

kj::Promise<void> runServer(kj::AsyncIoContext& ctx) {
  auto& network = ctx.provider->getNetwork();

  // Listen on port
  auto listener = network.parseAddress("*:8080")
      .wait(ctx.waitScope)
      ->listen();

  // Accept connections
  return listener->accept().then([](kj::Own<kj::AsyncIoStream> stream) {
    return handleConnection(kj::mv(stream));
  });
}`}</code></pre>

      <h3>TCP Client</h3>

      <pre><code>{`kj::Promise<void> connect(kj::AsyncIoContext& ctx) {
  auto& network = ctx.provider->getNetwork();

  return network.parseAddress("localhost:8080")
      .then([](kj::Own<kj::NetworkAddress> addr) {
        return addr->connect();
      })
      .then([](kj::Own<kj::AsyncIoStream> stream) {
        return useConnection(kj::mv(stream));
      });
}`}</code></pre>

      <h3>Reading and Writing</h3>

      <pre><code>{`kj::Promise<void> echo(kj::AsyncIoStream& stream) {
  auto buffer = kj::heapArray<kj::byte>(1024);

  return stream.read(buffer.begin(), 1, buffer.size())
      .then([&stream, buffer = kj::mv(buffer)](size_t n) mutable {
        return stream.write(buffer.begin(), n);
      })
      .then([&stream]() {
        return echo(stream);
      });
}`}</code></pre>

      <h2>Exception Handling</h2>

      <pre><code>{`#include <kj/exception.h>

// Throw with context
KJ_FAIL_REQUIRE("Invalid input", value, "expected positive");

// Assert
KJ_REQUIRE(value > 0, "Value must be positive", value);

// Debug assertions (removed in release)
KJ_DASSERT(pointer != nullptr);

// Catch and handle
try {
  riskyOperation();
} catch (const kj::Exception& e) {
  KJ_LOG(ERROR, "Operation failed", e.getDescription());
}`}</code></pre>

      <h2>Logging</h2>

      <pre><code>{`#include <kj/debug.h>

KJ_LOG(INFO, "Starting server", port);
KJ_LOG(WARNING, "Connection timeout", address);
KJ_LOG(ERROR, "Failed to read", filename, errno);

// Debug logging (can be compiled out)
KJ_DBG("Debug value", someVariable);`}</code></pre>

      <h2>Time and Timers</h2>

      <pre><code>{`#include <kj/async-io.h>
#include <kj/time.h>

kj::Promise<void> delayed(kj::Timer& timer) {
  // Sleep for 1 second
  return timer.afterDelay(1 * kj::SECONDS);
}

kj::Promise<void> timeout(kj::Timer& timer, kj::Promise<void> op) {
  // Timeout after 30 seconds
  return op.exclusiveJoin(
      timer.afterDelay(30 * kj::SECONDS)
          .then([]() -> kj::Promise<void> {
            KJ_FAIL_REQUIRE("Operation timed out");
          }));
}`}</code></pre>

      <h2>Thread Safety</h2>

      <p>
        KJ async is single-threaded by design. Use executors for multi-threading:
      </p>

      <pre><code>{`#include <kj/async.h>
#include <kj/thread.h>

// Run work on another thread
auto executor = kj::getCurrentThreadExecutor();

kj::Thread thread([&]() {
  // This runs on another thread
  auto promise = executor.executeAsync([&]() {
    // This runs back on the main thread
    return result;
  });
});`}</code></pre>

      <h2>Next Steps</h2>

      <ul>
        <li>See <a href="/docs/examples">examples</a></li>
        <li>Learn about the <a href="/docs/rpc">RPC system</a></li>
      </ul>
    </DocsLayout>
  )
}
