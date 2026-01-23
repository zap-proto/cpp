import { DocsLayout } from '@/components/docs-layout'

export default function RPC() {
  return (
    <DocsLayout>
      <h1>RPC System</h1>

      <p>
        ZAP C++ includes a capability-based RPC system with promise pipelining,
        allowing efficient distributed communication.
      </p>

      <h2>Core Concepts</h2>

      <h3>Capabilities</h3>
      <p>
        A capability is a reference to a remote object. If you have a capability,
        you can call methods on it. This is the object-capability security model.
      </p>

      <h3>Promise Pipelining</h3>
      <p>
        When a method returns a capability, you can immediately call methods on
        that capability without waiting for the first call to complete. The
        system automatically pipelines requests.
      </p>

      <pre><code>{`// Without pipelining: 3 round trips
User user = await session.login(creds);
Profile profile = await user.getProfile();
Avatar avatar = await profile.getAvatar();

// With pipelining: 1 round trip
Promise<Avatar> avatar = session.login(creds)
                                .getProfile()
                                .getAvatar();`}</code></pre>

      <h2>Defining Interfaces</h2>

      <pre><code>{`# calculator.capnp
@0x85150b117366d14b;

interface Calculator {
  add @0 (a :Float64, b :Float64) -> (result :Float64);
  subtract @1 (a :Float64, b :Float64) -> (result :Float64);
  multiply @2 (a :Float64, b :Float64) -> (result :Float64);
  divide @3 (a :Float64, b :Float64) -> (result :Float64);

  # Returns a new calculator that adds offset to all results
  withOffset @4 (offset :Float64) -> (calc :Calculator);
}`}</code></pre>

      <h2>Implementing Servers</h2>

      <pre><code>{`#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>
#include "calculator.capnp.h"

class CalculatorImpl final : public Calculator::Server {
public:
  kj::Promise<void> add(AddContext context) override {
    auto params = context.getParams();
    auto result = params.getA() + params.getB();
    context.getResults().setResult(result);
    return kj::READY_NOW;
  }

  kj::Promise<void> subtract(SubtractContext context) override {
    auto params = context.getParams();
    auto result = params.getA() - params.getB();
    context.getResults().setResult(result);
    return kj::READY_NOW;
  }

  kj::Promise<void> multiply(MultiplyContext context) override {
    auto params = context.getParams();
    auto result = params.getA() * params.getB();
    context.getResults().setResult(result);
    return kj::READY_NOW;
  }

  kj::Promise<void> divide(DivideContext context) override {
    auto params = context.getParams();
    if (params.getB() == 0) {
      KJ_FAIL_REQUIRE("Division by zero");
    }
    auto result = params.getA() / params.getB();
    context.getResults().setResult(result);
    return kj::READY_NOW;
  }

  kj::Promise<void> withOffset(WithOffsetContext context) override {
    auto offset = context.getParams().getOffset();
    context.getResults().setCalc(
        kj::heap<OffsetCalculator>(offset));
    return kj::READY_NOW;
  }
};`}</code></pre>

      <h2>Running a Server</h2>

      <pre><code>{`#include <capnp/ez-rpc.h>

int main() {
  // Create the event loop
  kj::AsyncIoContext ctx = kj::setupAsyncIo();

  // Create server capability
  Calculator::Client calculator = kj::heap<CalculatorImpl>();

  // Start RPC server
  capnp::EzRpcServer server(
      kj::heap<CalculatorImpl>(),
      "localhost:5000");

  // Run forever
  auto& waitScope = server.getWaitScope();
  kj::NEVER_DONE.wait(waitScope);

  return 0;
}`}</code></pre>

      <h2>Creating Clients</h2>

      <pre><code>{`#include <capnp/ez-rpc.h>
#include "calculator.capnp.h"

int main() {
  // Connect to server
  capnp::EzRpcClient client("localhost:5000");
  auto& waitScope = client.getWaitScope();

  // Get the calculator capability
  Calculator::Client calc = client.getMain<Calculator>();

  // Make a request
  auto request = calc.addRequest();
  request.setA(5.0);
  request.setB(3.0);

  // Wait for result
  auto response = request.send().wait(waitScope);
  double result = response.getResult();
  // result == 8.0

  return 0;
}`}</code></pre>

      <h2>Promise Pipelining</h2>

      <pre><code>{`// Get a calculator with offset, then use it
auto offsetCalc = calc.withOffsetRequest();
offsetCalc.setOffset(10.0);
auto promise = offsetCalc.send();

// Pipeline: call add on the returned calculator
// This doesn't wait for withOffset to complete
auto addRequest = promise.getCalc().addRequest();
addRequest.setA(1.0);
addRequest.setB(2.0);

// Only one round-trip for both calls!
auto result = addRequest.send().wait(waitScope);
// result.getResult() == 13.0 (1 + 2 + 10)`}</code></pre>

      <h2>Error Handling</h2>

      <pre><code>{`try {
  auto request = calc.divideRequest();
  request.setA(10.0);
  request.setB(0.0);

  auto response = request.send().wait(waitScope);
  // This throws because the server failed
} catch (const kj::Exception& e) {
  // e.getDescription() contains error message
  std::cerr << "RPC failed: " << e.getDescription().cStr() << std::endl;
}`}</code></pre>

      <h2>Cancellation</h2>

      <pre><code>{`// Start a long operation
auto request = server.longOperationRequest();
auto promise = request.send();

// Cancel it
promise = nullptr;  // Dropping the promise cancels the request`}</code></pre>

      <h2>Streaming</h2>

      <pre><code>{`# Schema for streaming
interface DataStream {
  # Called for each chunk
  write @0 (data :Data) -> stream;

  # Called when done
  done @1 () -> ();
}

interface DataProducer {
  # Start streaming to the given callback
  stream @0 (callback :DataStream) -> ();
}`}</code></pre>

      <pre><code>{`// Server implementation
class DataStreamImpl : public DataStream::Server {
  kj::Promise<void> write(WriteContext context) override {
    auto data = context.getParams().getData();
    processChunk(data);
    return kj::READY_NOW;
  }

  kj::Promise<void> done(DoneContext context) override {
    finalize();
    return kj::READY_NOW;
  }
};`}</code></pre>

      <h2>Two-Party Protocol</h2>

      <p>For direct connections without a central server:</p>

      <pre><code>{`#include <capnp/rpc-twoparty.h>
#include <kj/async-io.h>

// Server side
void runServer(kj::AsyncIoStream& stream) {
  capnp::TwoPartyServer server(kj::heap<MyServiceImpl>());

  auto& network = server.accept(stream);
  // Connection is now active
}

// Client side
void runClient(kj::AsyncIoStream& stream) {
  capnp::TwoPartyClient client(stream);

  auto cap = client.bootstrap().castAs<MyService>();
  // Use the capability
}`}</code></pre>

      <h2>Best Practices</h2>

      <ol>
        <li>
          <strong>Use promise pipelining</strong> - Avoid unnecessary round trips
        </li>
        <li>
          <strong>Return capabilities</strong> - Instead of IDs, return object references
        </li>
        <li>
          <strong>Cancel unused promises</strong> - Free server resources early
        </li>
        <li>
          <strong>Handle errors</strong> - Always catch exceptions from RPC calls
        </li>
        <li>
          <strong>Use streaming for large data</strong> - Don't send huge messages
        </li>
      </ol>

      <h2>Next Steps</h2>

      <ul>
        <li>See <a href="/docs/rpc-examples">RPC examples</a></li>
        <li>Learn about the <a href="/docs/kj">KJ async library</a></li>
      </ul>
    </DocsLayout>
  )
}
