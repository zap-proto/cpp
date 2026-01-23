import { DocsLayout } from '@/components/docs-layout'

export default function RPCExamples() {
  return (
    <DocsLayout>
      <h1>RPC Examples</h1>

      <p>
        This page contains practical examples of using the ZAP C++ RPC system.
      </p>

      <h2>Echo Server</h2>

      <h3>Schema</h3>

      <pre><code>{`# echo.capnp
@0x8e5322c1e9282534;

interface Echo {
  echo @0 (message :Text) -> (reply :Text);
}`}</code></pre>

      <h3>Server Implementation</h3>

      <pre><code>{`// echo_server.cpp
#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <kj/debug.h>
#include <iostream>
#include "echo.capnp.h"

class EchoImpl final : public Echo::Server {
public:
  kj::Promise<void> echo(EchoContext context) override {
    auto message = context.getParams().getMessage();
    KJ_LOG(INFO, "Received", message);

    context.getResults().setReply(kj::str("Echo: ", message));
    return kj::READY_NOW;
  }
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <address:port>\\n";
    return 1;
  }

  // Start server
  capnp::EzRpcServer server(kj::heap<EchoImpl>(), argv[1]);
  auto& waitScope = server.getWaitScope();

  std::cout << "Listening on " << argv[1] << "...\\n";

  // Run forever
  kj::NEVER_DONE.wait(waitScope);
}`}</code></pre>

      <h3>Client Implementation</h3>

      <pre><code>{`// echo_client.cpp
#include <capnp/ez-rpc.h>
#include <iostream>
#include "echo.capnp.h"

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <address:port> <message>\\n";
    return 1;
  }

  // Connect to server
  capnp::EzRpcClient client(argv[1]);
  auto& waitScope = client.getWaitScope();

  // Get the Echo capability
  Echo::Client echo = client.getMain<Echo>();

  // Make request
  auto request = echo.echoRequest();
  request.setMessage(argv[2]);

  // Wait for response
  auto response = request.send().wait(waitScope);
  std::cout << response.getReply().cStr() << "\\n";

  return 0;
}`}</code></pre>

      <h2>Calculator with Pipelining</h2>

      <h3>Schema</h3>

      <pre><code>{`# calculator.capnp
@0xf9e5a3d2c1b08765;

interface Calculator {
  # Basic operations
  evaluate @0 (expression :Expression) -> (value :Float64);

  # Get a calculator function as a capability
  getOperator @1 (op :Operator) -> (func :Function);

  enum Operator {
    add @0;
    subtract @1;
    multiply @2;
    divide @3;
  }
}

interface Function {
  call @0 (params :List(Float64)) -> (value :Float64);
}

struct Expression {
  union {
    literal @0 :Float64;
    call :group {
      function @1 :Function;
      params @2 :List(Expression);
    }
  }
}`}</code></pre>

      <h3>Server with Pipelining</h3>

      <pre><code>{`// calculator_server.cpp
#include <capnp/ez-rpc.h>
#include "calculator.capnp.h"

class FunctionImpl final : public Function::Server {
public:
  using Op = std::function<double(double, double)>;

  FunctionImpl(Op op) : op(kj::mv(op)) {}

  kj::Promise<void> call(CallContext context) override {
    auto params = context.getParams().getParams();
    if (params.size() < 2) {
      KJ_FAIL_REQUIRE("Function requires at least 2 parameters");
    }

    double result = params[0];
    for (size_t i = 1; i < params.size(); i++) {
      result = op(result, params[i]);
    }

    context.getResults().setValue(result);
    return kj::READY_NOW;
  }

private:
  Op op;
};

class CalculatorImpl final : public Calculator::Server {
public:
  kj::Promise<void> evaluate(EvaluateContext context) override {
    auto expr = context.getParams().getExpression();
    double result = evaluateExpr(expr);
    context.getResults().setValue(result);
    return kj::READY_NOW;
  }

  kj::Promise<void> getOperator(GetOperatorContext context) override {
    auto op = context.getParams().getOp();
    Function::Client func;

    switch (op) {
      case Calculator::Operator::ADD:
        func = kj::heap<FunctionImpl>([](double a, double b) { return a + b; });
        break;
      case Calculator::Operator::SUBTRACT:
        func = kj::heap<FunctionImpl>([](double a, double b) { return a - b; });
        break;
      case Calculator::Operator::MULTIPLY:
        func = kj::heap<FunctionImpl>([](double a, double b) { return a * b; });
        break;
      case Calculator::Operator::DIVIDE:
        func = kj::heap<FunctionImpl>([](double a, double b) { return a / b; });
        break;
    }

    context.getResults().setFunc(kj::mv(func));
    return kj::READY_NOW;
  }

private:
  double evaluateExpr(Expression::Reader expr) {
    switch (expr.which()) {
      case Expression::LITERAL:
        return expr.getLiteral();
      case Expression::CALL:
        KJ_FAIL_REQUIRE("Nested calls not yet implemented");
    }
    KJ_UNREACHABLE;
  }
};`}</code></pre>

      <h3>Client with Pipelining</h3>

      <pre><code>{`// calculator_client.cpp
#include <capnp/ez-rpc.h>
#include <iostream>
#include "calculator.capnp.h"

int main() {
  capnp::EzRpcClient client("localhost:5000");
  auto& waitScope = client.getWaitScope();

  Calculator::Client calc = client.getMain<Calculator>();

  // Get the multiply operator - this returns a Function capability
  auto getOpRequest = calc.getOperatorRequest();
  getOpRequest.setOp(Calculator::Operator::MULTIPLY);
  auto opPromise = getOpRequest.send();

  // Pipeline: immediately call the function without waiting
  // for getOperator to complete!
  auto callRequest = opPromise.getFunc().callRequest();
  auto params = callRequest.initParams(3);
  params.set(0, 2.0);
  params.set(1, 3.0);
  params.set(2, 4.0);

  // Both calls are sent in a single round-trip
  auto result = callRequest.send().wait(waitScope);

  std::cout << "2 * 3 * 4 = " << result.getValue() << "\\n";
  // Output: 2 * 3 * 4 = 24

  return 0;
}`}</code></pre>

      <h2>Streaming Data</h2>

      <h3>Schema</h3>

      <pre><code>{`# streaming.capnp
@0xb4c1d2e3f4a59687;

interface DataProducer {
  # Subscribe to data stream
  subscribe @0 (subscriber :DataSubscriber) -> ();
}

interface DataSubscriber {
  # Called for each data point
  onData @0 (timestamp :Int64, value :Float64) -> ();

  # Called when stream ends
  onComplete @1 () -> ();

  # Called on error
  onError @2 (message :Text) -> ();
}`}</code></pre>

      <h3>Producer Implementation</h3>

      <pre><code>{`// producer.cpp
#include <capnp/ez-rpc.h>
#include <kj/async-io.h>
#include "streaming.capnp.h"

class DataProducerImpl final : public DataProducer::Server {
public:
  DataProducerImpl(kj::Timer& timer) : timer(timer) {}

  kj::Promise<void> subscribe(SubscribeContext context) override {
    auto subscriber = context.getParams().getSubscriber();
    return streamData(kj::mv(subscriber), 0);
  }

private:
  kj::Promise<void> streamData(DataSubscriber::Client subscriber, int count) {
    if (count >= 100) {
      // Send completion
      return subscriber.onCompleteRequest().send().ignoreResult();
    }

    // Send data point
    auto request = subscriber.onDataRequest();
    request.setTimestamp(kj::systemPreciseMonotonicClock().now()
                             .nanoseconds / 1000000);
    request.setValue(sin(count * 0.1) * 100);

    return request.send()
        .then([this, subscriber = kj::mv(subscriber), count]() mutable {
          // Wait 100ms then send next
          return timer.afterDelay(100 * kj::MILLISECONDS)
              .then([this, subscriber = kj::mv(subscriber), count]() mutable {
                return streamData(kj::mv(subscriber), count + 1);
              });
        });
  }

  kj::Timer& timer;
};`}</code></pre>

      <h3>Subscriber Implementation</h3>

      <pre><code>{`// subscriber.cpp
#include <capnp/ez-rpc.h>
#include <iostream>
#include "streaming.capnp.h"

class DataSubscriberImpl final : public DataSubscriber::Server {
public:
  kj::Promise<void> onData(OnDataContext context) override {
    auto params = context.getParams();
    std::cout << "Data: timestamp=" << params.getTimestamp()
              << " value=" << params.getValue() << "\\n";
    return kj::READY_NOW;
  }

  kj::Promise<void> onComplete(OnCompleteContext context) override {
    std::cout << "Stream complete\\n";
    return kj::READY_NOW;
  }

  kj::Promise<void> onError(OnErrorContext context) override {
    std::cerr << "Error: " << context.getParams().getMessage().cStr() << "\\n";
    return kj::READY_NOW;
  }
};

int main() {
  capnp::EzRpcClient client("localhost:5000");
  auto& waitScope = client.getWaitScope();

  DataProducer::Client producer = client.getMain<DataProducer>();

  // Subscribe with our callback
  auto request = producer.subscribeRequest();
  request.setSubscriber(kj::heap<DataSubscriberImpl>());
  request.send().wait(waitScope);

  return 0;
}`}</code></pre>

      <h2>Bidirectional Communication</h2>

      <h3>Schema</h3>

      <pre><code>{`# chat.capnp
@0xc5d4e3f2a1b09876;

interface ChatServer {
  join @0 (username :Text, client :ChatClient) -> (session :ChatSession);
}

interface ChatSession {
  send @0 (message :Text) -> ();
  leave @1 () -> ();
}

interface ChatClient {
  receive @0 (from :Text, message :Text) -> ();
  userJoined @1 (username :Text) -> ();
  userLeft @2 (username :Text) -> ();
}`}</code></pre>

      <h3>Server</h3>

      <pre><code>{`// chat_server.cpp
class ChatSessionImpl final : public ChatSession::Server {
public:
  ChatSessionImpl(kj::String username, ChatRoom& room)
      : username(kj::mv(username)), room(room) {}

  kj::Promise<void> send(SendContext context) override {
    auto message = context.getParams().getMessage();
    room.broadcast(username, message);
    return kj::READY_NOW;
  }

  kj::Promise<void> leave(LeaveContext context) override {
    room.leave(username);
    return kj::READY_NOW;
  }

private:
  kj::String username;
  ChatRoom& room;
};

class ChatServerImpl final : public ChatServer::Server {
public:
  kj::Promise<void> join(JoinContext context) override {
    auto username = kj::heapString(context.getParams().getUsername());
    auto client = context.getParams().getClient();

    room.join(username, kj::mv(client));

    context.getResults().setSession(
        kj::heap<ChatSessionImpl>(kj::mv(username), room));
    return kj::READY_NOW;
  }

private:
  ChatRoom room;
};`}</code></pre>

      <h2>CMakeLists.txt for RPC Examples</h2>

      <pre><code>{`cmake_minimum_required(VERSION 3.16)
project(rpc_examples CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CapnProto REQUIRED)

# Generate C++ from schemas
capnp_generate_cpp(ECHO_SRCS ECHO_HDRS echo.capnp)
capnp_generate_cpp(CALC_SRCS CALC_HDRS calculator.capnp)
capnp_generate_cpp(STREAM_SRCS STREAM_HDRS streaming.capnp)

# Echo server and client
add_executable(echo_server echo_server.cpp \${ECHO_SRCS})
target_include_directories(echo_server PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(echo_server PRIVATE
  CapnProto::capnp
  CapnProto::capnp-rpc
  CapnProto::kj
  CapnProto::kj-async
)

add_executable(echo_client echo_client.cpp \${ECHO_SRCS})
target_include_directories(echo_client PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(echo_client PRIVATE
  CapnProto::capnp
  CapnProto::capnp-rpc
  CapnProto::kj
  CapnProto::kj-async
)

# Calculator with pipelining
add_executable(calc_server calculator_server.cpp \${CALC_SRCS})
target_include_directories(calc_server PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(calc_server PRIVATE
  CapnProto::capnp
  CapnProto::capnp-rpc
  CapnProto::kj
  CapnProto::kj-async
)`}</code></pre>

      <h2>Running the Examples</h2>

      <pre><code>{`# Build
mkdir build && cd build
cmake ..
cmake --build .

# Run echo example
./echo_server localhost:5000 &
./echo_client localhost:5000 "Hello, World!"
# Output: Echo: Hello, World!

# Run calculator example
./calc_server localhost:5001 &
./calc_client localhost:5001`}</code></pre>

      <h2>Next Steps</h2>

      <ul>
        <li>Learn more about the <a href="/docs/rpc">RPC system</a></li>
        <li>Explore the <a href="/docs/kj">KJ async library</a></li>
      </ul>
    </DocsLayout>
  )
}
