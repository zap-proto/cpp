import { DocsLayout } from '@/components/docs-layout'

export default function Building() {
  return (
    <DocsLayout>
      <h1>Building Projects with ZAP C++</h1>

      <p>
        This guide explains how to integrate ZAP C++ into your CMake projects
        and compile schema files.
      </p>

      <h2>Project Structure</h2>

      <p>A typical ZAP C++ project structure:</p>

      <pre><code>{`myproject/
├── CMakeLists.txt
├── schemas/
│   └── message.capnp
├── src/
│   └── main.cpp
└── include/
    └── myproject/`}</code></pre>

      <h2>CMake Integration</h2>

      <h3>Finding the Package</h3>

      <pre><code>{`# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(myproject CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ZAP C++ (Cap'n Proto)
find_package(CapnProto REQUIRED)

add_executable(myapp src/main.cpp)
target_link_libraries(myapp PRIVATE CapnProto::capnp)`}</code></pre>

      <h3>Compiling Schema Files</h3>

      <p>Use the <code>capnp_generate_cpp</code> function to compile schemas:</p>

      <pre><code>{`# CMakeLists.txt
find_package(CapnProto REQUIRED)

# Generate C++ from schema
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS
  schemas/message.capnp
  schemas/protocol.capnp
)

add_executable(myapp
  src/main.cpp
  \${CAPNP_SRCS}
)

target_include_directories(myapp PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(myapp PRIVATE CapnProto::capnp)`}</code></pre>

      <h3>Using RPC</h3>

      <p>For RPC support, link against additional libraries:</p>

      <pre><code>{`# Link RPC libraries
target_link_libraries(myapp PRIVATE
  CapnProto::capnp
  CapnProto::capnp-rpc
  CapnProto::kj
  CapnProto::kj-async
)`}</code></pre>

      <h2>Manual Compilation</h2>

      <p>You can also compile schemas manually:</p>

      <pre><code>{`# Compile to C++
capnp compile -oc++ schemas/message.capnp

# This generates:
#   schemas/message.capnp.h
#   schemas/message.capnp.c++

# Specify output directory
capnp compile -oc++:build/generated schemas/message.capnp

# Include search paths
capnp compile -oc++ -I/path/to/imports schemas/message.capnp`}</code></pre>

      <h2>Build Configuration</h2>

      <h3>Debug vs Release</h3>

      <pre><code>{`# Debug build (with assertions)
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# Release build (optimized)
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release`}</code></pre>

      <h3>Compiler Flags</h3>

      <p>Recommended compiler flags for production:</p>

      <pre><code>{`# CMakeLists.txt
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(myapp PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
  )
endif()

# Enable sanitizers in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(myapp PRIVATE -fsanitize=address,undefined)
  target_link_options(myapp PRIVATE -fsanitize=address,undefined)
endif()`}</code></pre>

      <h2>FetchContent Integration</h2>

      <p>Include ZAP C++ directly in your project:</p>

      <pre><code>{`# CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
  capnproto
  GIT_REPOSITORY https://github.com/zap-protocol/zap-cpp.git
  GIT_TAG        v1.0.0
)

FetchContent_MakeAvailable(capnproto)

add_executable(myapp src/main.cpp)
target_link_libraries(myapp PRIVATE CapnProto::capnp)`}</code></pre>

      <h2>Complete Example</h2>

      <h3>Schema File</h3>

      <pre><code>{`# schemas/person.capnp
@0xdbb9ad1f14bf0b36;

struct Person {
  name @0 :Text;
  age @1 :UInt32;
  email @2 :Text;

  struct PhoneNumber {
    number @0 :Text;
    type @1 :Type;

    enum Type {
      mobile @0;
      home @1;
      work @2;
    }
  }

  phones @3 :List(PhoneNumber);
}`}</code></pre>

      <h3>CMakeLists.txt</h3>

      <pre><code>{`cmake_minimum_required(VERSION 3.16)
project(person_example CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CapnProto REQUIRED)

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS
  schemas/person.capnp
)

add_executable(person_example
  src/main.cpp
  \${CAPNP_SRCS}
)

target_include_directories(person_example PRIVATE
  \${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(person_example PRIVATE
  CapnProto::capnp
)`}</code></pre>

      <h3>Source File</h3>

      <pre><code>{`// src/main.cpp
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <iostream>
#include "schemas/person.capnp.h"

int main() {
  // Build a message
  capnp::MallocMessageBuilder message;
  auto person = message.initRoot<Person>();

  person.setName("Alice");
  person.setAge(30);
  person.setEmail("alice@example.com");

  auto phones = person.initPhones(2);
  phones[0].setNumber("+1-555-1234");
  phones[0].setType(Person::PhoneNumber::Type::MOBILE);
  phones[1].setNumber("+1-555-5678");
  phones[1].setType(Person::PhoneNumber::Type::WORK);

  // Serialize to stdout
  capnp::writePackedMessageToFd(1, message);

  return 0;
}`}</code></pre>

      <h3>Build and Run</h3>

      <pre><code>{`mkdir build && cd build
cmake ..
cmake --build .
./person_example | capnp decode ../schemas/person.capnp Person`}</code></pre>

      <h2>IDE Integration</h2>

      <h3>VS Code</h3>

      <p>Add to <code>.vscode/settings.json</code>:</p>

      <pre><code>{`{
  "files.associations": {
    "*.capnp": "capnproto"
  },
  "cmake.configureArgs": [
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  ]
}`}</code></pre>

      <h3>CLion</h3>

      <p>
        CLion automatically detects CMake projects. For schema syntax highlighting,
        install the Cap'n Proto plugin from the JetBrains marketplace.
      </p>

      <h2>Next Steps</h2>

      <ul>
        <li>Learn the <a href="/docs/schema">schema language</a></li>
        <li>Explore the <a href="/docs/api">C++ API</a></li>
        <li>See more <a href="/docs/examples">examples</a></li>
      </ul>
    </DocsLayout>
  )
}
