import { DocsLayout } from '@/components/docs-layout'

export default function Examples() {
  return (
    <DocsLayout>
      <h1>Examples</h1>

      <p>
        This page contains practical examples of using ZAP C++ for serialization.
      </p>

      <h2>Basic Serialization</h2>

      <h3>Schema</h3>

      <pre><code>{`# addressbook.capnp
@0xdbb9ad1f14bf0b36;

struct Person {
  id @0 :UInt32;
  name @1 :Text;
  email @2 :Text;
  phones @3 :List(PhoneNumber);

  struct PhoneNumber {
    number @0 :Text;
    type @1 :Type;

    enum Type {
      mobile @0;
      home @1;
      work @2;
    }
  }
}

struct AddressBook {
  people @0 :List(Person);
}`}</code></pre>

      <h3>Writing Data</h3>

      <pre><code>{`#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <fcntl.h>
#include "addressbook.capnp.h"

void writeAddressBook(int fd) {
  capnp::MallocMessageBuilder message;

  auto addressBook = message.initRoot<AddressBook>();
  auto people = addressBook.initPeople(2);

  // First person
  auto alice = people[0];
  alice.setId(123);
  alice.setName("Alice");
  alice.setEmail("alice@example.com");

  auto alicePhones = alice.initPhones(1);
  alicePhones[0].setNumber("+1-555-1234");
  alicePhones[0].setType(Person::PhoneNumber::Type::MOBILE);

  // Second person
  auto bob = people[1];
  bob.setId(456);
  bob.setName("Bob");
  bob.setEmail("bob@example.com");

  auto bobPhones = bob.initPhones(2);
  bobPhones[0].setNumber("+1-555-5678");
  bobPhones[0].setType(Person::PhoneNumber::Type::HOME);
  bobPhones[1].setNumber("+1-555-9999");
  bobPhones[1].setType(Person::PhoneNumber::Type::WORK);

  // Write to file
  capnp::writePackedMessageToFd(fd, message);
}`}</code></pre>

      <h3>Reading Data</h3>

      <pre><code>{`#include <capnp/serialize-packed.h>
#include <iostream>
#include "addressbook.capnp.h"

void readAddressBook(int fd) {
  capnp::PackedFdMessageReader message(fd);

  auto addressBook = message.getRoot<AddressBook>();

  for (auto person : addressBook.getPeople()) {
    std::cout << person.getName().cStr()
              << " (id=" << person.getId() << ")\\n";
    std::cout << "  Email: " << person.getEmail().cStr() << "\\n";

    for (auto phone : person.getPhones()) {
      const char* typeName;
      switch (phone.getType()) {
        case Person::PhoneNumber::Type::MOBILE:
          typeName = "mobile";
          break;
        case Person::PhoneNumber::Type::HOME:
          typeName = "home";
          break;
        case Person::PhoneNumber::Type::WORK:
          typeName = "work";
          break;
      }
      std::cout << "  Phone (" << typeName << "): "
                << phone.getNumber().cStr() << "\\n";
    }
  }
}`}</code></pre>

      <h2>Using Unions</h2>

      <h3>Schema</h3>

      <pre><code>{`# shape.capnp
@0xa1b2c3d4e5f67890;

struct Shape {
  name @0 :Text;

  union {
    circle @1 :Circle;
    rectangle @2 :Rectangle;
    triangle @3 :Triangle;
  }

  struct Circle {
    radius @0 :Float64;
  }

  struct Rectangle {
    width @0 :Float64;
    height @1 :Float64;
  }

  struct Triangle {
    base @0 :Float64;
    height @1 :Float64;
  }
}

struct Drawing {
  shapes @0 :List(Shape);
}`}</code></pre>

      <h3>Working with Unions</h3>

      <pre><code>{`#include "shape.capnp.h"
#include <cmath>

// Calculate area based on shape type
double calculateArea(Shape::Reader shape) {
  switch (shape.which()) {
    case Shape::CIRCLE: {
      auto circle = shape.getCircle();
      return M_PI * circle.getRadius() * circle.getRadius();
    }
    case Shape::RECTANGLE: {
      auto rect = shape.getRectangle();
      return rect.getWidth() * rect.getHeight();
    }
    case Shape::TRIANGLE: {
      auto tri = shape.getTriangle();
      return 0.5 * tri.getBase() * tri.getHeight();
    }
  }
  return 0;
}

// Create shapes
void createShapes(capnp::MallocMessageBuilder& message) {
  auto drawing = message.initRoot<Drawing>();
  auto shapes = drawing.initShapes(3);

  // Circle
  shapes[0].setName("Circle A");
  auto circle = shapes[0].initCircle();
  circle.setRadius(5.0);

  // Rectangle
  shapes[1].setName("Rectangle B");
  auto rect = shapes[1].initRectangle();
  rect.setWidth(10.0);
  rect.setHeight(20.0);

  // Triangle
  shapes[2].setName("Triangle C");
  auto tri = shapes[2].initTriangle();
  tri.setBase(8.0);
  tri.setHeight(6.0);
}`}</code></pre>

      <h2>JSON Conversion</h2>

      <pre><code>{`#include <capnp/compat/json.h>
#include <capnp/message.h>
#include <iostream>

void convertToJson(Person::Reader person) {
  capnp::JsonCodec codec;
  kj::String json = codec.encode(person);
  std::cout << json.cStr() << std::endl;
}

void parseFromJson(kj::StringPtr json, capnp::MallocMessageBuilder& message) {
  capnp::JsonCodec codec;
  auto person = message.initRoot<Person>();
  codec.decode(json, person);
}`}</code></pre>

      <h2>Memory-Mapped Files</h2>

      <pre><code>{`#include <capnp/serialize.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class MappedMessage {
public:
  MappedMessage(const char* filename) {
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
      KJ_FAIL_SYSCALL("open", errno, filename);
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
      close(fd);
      KJ_FAIL_SYSCALL("fstat", errno);
    }
    size = st.st_size;

    data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
      close(fd);
      KJ_FAIL_SYSCALL("mmap", errno);
    }

    // Create reader directly from mapped memory
    words = kj::ArrayPtr<const capnp::word>(
        reinterpret_cast<const capnp::word*>(data),
        size / sizeof(capnp::word));
    reader = kj::heap<capnp::FlatArrayMessageReader>(words);
  }

  ~MappedMessage() {
    reader = nullptr;
    if (data != MAP_FAILED) munmap(data, size);
    if (fd >= 0) close(fd);
  }

  template <typename T>
  typename T::Reader getRoot() {
    return reader->getRoot<T>();
  }

private:
  int fd = -1;
  void* data = MAP_FAILED;
  size_t size = 0;
  kj::ArrayPtr<const capnp::word> words;
  kj::Own<capnp::FlatArrayMessageReader> reader;
};

// Usage
void useMappedFile() {
  MappedMessage msg("large-data.capnp");
  auto data = msg.getRoot<LargeDataSet>();
  // Access data directly from disk without copying
}`}</code></pre>

      <h2>Streaming Messages</h2>

      <pre><code>{`#include <capnp/serialize.h>
#include <kj/io.h>

// Write multiple messages to a stream
void writeStream(kj::OutputStream& output,
                 kj::ArrayPtr<const LogEntry::Reader> entries) {
  for (auto entry : entries) {
    capnp::MallocMessageBuilder message;
    message.setRoot(entry);
    capnp::writeMessage(output, message);
  }
}

// Read multiple messages from a stream
kj::Vector<capnp::MallocMessageBuilder> readStream(kj::InputStream& input) {
  kj::Vector<capnp::MallocMessageBuilder> messages;

  while (true) {
    KJ_IF_MAYBE(reader, capnp::tryReadMessage(input)) {
      auto& msg = messages.add();
      msg.setRoot(reader->getRoot<LogEntry>());
    } else {
      break;
    }
  }

  return messages;
}`}</code></pre>

      <h2>Dynamic Message Access</h2>

      <pre><code>{`#include <capnp/dynamic.h>
#include <capnp/schema.h>
#include <capnp/schema-loader.h>

void printDynamic(capnp::DynamicStruct::Reader reader) {
  auto schema = reader.getSchema();

  for (auto field : schema.getFields()) {
    auto name = field.getProto().getName();
    auto value = reader.get(field);

    std::cout << name.cStr() << ": ";

    switch (value.getType()) {
      case capnp::DynamicValue::INT:
        std::cout << value.as<int64_t>();
        break;
      case capnp::DynamicValue::UINT:
        std::cout << value.as<uint64_t>();
        break;
      case capnp::DynamicValue::FLOAT:
        std::cout << value.as<double>();
        break;
      case capnp::DynamicValue::TEXT:
        std::cout << '"' << value.as<capnp::Text>().cStr() << '"';
        break;
      case capnp::DynamicValue::STRUCT:
        std::cout << "{...}";
        break;
      case capnp::DynamicValue::LIST:
        std::cout << "[...]";
        break;
      default:
        std::cout << "(unknown)";
    }
    std::cout << "\\n";
  }
}`}</code></pre>

      <h2>CMakeLists.txt</h2>

      <pre><code>{`cmake_minimum_required(VERSION 3.16)
project(examples CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CapnProto REQUIRED)

# Generate C++ from schemas
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS
  addressbook.capnp
  shape.capnp
)

# Address book example
add_executable(addressbook
  addressbook_main.cpp
  \${CAPNP_SRCS}
)
target_include_directories(addressbook PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(addressbook PRIVATE CapnProto::capnp)

# Shape example
add_executable(shapes
  shapes_main.cpp
  \${CAPNP_SRCS}
)
target_include_directories(shapes PRIVATE \${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(shapes PRIVATE CapnProto::capnp)`}</code></pre>

      <h2>Next Steps</h2>

      <ul>
        <li>See <a href="/docs/rpc-examples">RPC examples</a></li>
        <li>Learn about the <a href="/docs/api">C++ API</a></li>
      </ul>
    </DocsLayout>
  )
}
