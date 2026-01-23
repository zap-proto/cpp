import { DocsLayout } from '@/components/docs-layout'

export default function API() {
  return (
    <DocsLayout>
      <h1>C++ API Reference</h1>

      <p>
        This reference covers the core ZAP C++ (Cap'n Proto) API for building
        and reading messages.
      </p>

      <h2>Headers</h2>

      <pre><code>{`#include <capnp/message.h>          // MessageBuilder, MessageReader
#include <capnp/serialize.h>         // writeMessageToFd, readMessageFromFd
#include <capnp/serialize-packed.h>  // Packed serialization
#include <capnp/pretty-print.h>      // Text output for debugging`}</code></pre>

      <h2>Building Messages</h2>

      <h3>MallocMessageBuilder</h3>

      <p>The primary class for building messages:</p>

      <pre><code>{`#include <capnp/message.h>
#include "person.capnp.h"

// Create a message builder
capnp::MallocMessageBuilder message;

// Get the root object
auto person = message.initRoot<Person>();

// Set scalar fields
person.setName("Alice");
person.setAge(30);

// Initialize and set list fields
auto phones = person.initPhones(2);
phones[0].setNumber("+1-555-1234");
phones[1].setNumber("+1-555-5678");`}</code></pre>

      <h3>Builder Interface</h3>

      <p>Generated builder classes provide these methods:</p>

      <table>
        <thead>
          <tr>
            <th>Method</th>
            <th>Description</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><code>setField(value)</code></td>
            <td>Set a scalar field</td>
          </tr>
          <tr>
            <td><code>initField()</code></td>
            <td>Initialize a struct field</td>
          </tr>
          <tr>
            <td><code>initField(size)</code></td>
            <td>Initialize a list field with given size</td>
          </tr>
          <tr>
            <td><code>getField()</code></td>
            <td>Get a builder for a struct field</td>
          </tr>
          <tr>
            <td><code>hasField()</code></td>
            <td>Check if a pointer field is set</td>
          </tr>
          <tr>
            <td><code>adoptField(orphan)</code></td>
            <td>Adopt an orphan into this field</td>
          </tr>
          <tr>
            <td><code>disownField()</code></td>
            <td>Remove and return field as orphan</td>
          </tr>
        </tbody>
      </table>

      <h3>Text and Data Fields</h3>

      <pre><code>{`// Setting text
person.setName("Alice");
person.setName(kj::StringPtr("Alice"));

// Setting data
auto data = person.initBinaryData(1024);
memcpy(data.begin(), buffer, 1024);

// From existing array
person.setData(kj::arrayPtr(buffer, size));`}</code></pre>

      <h3>List Fields</h3>

      <pre><code>{`// Initialize a list
auto phones = person.initPhones(3);
phones[0].setNumber("111");
phones[1].setNumber("222");
phones[2].setNumber("333");

// Set from existing data
kj::ArrayPtr<const kj::StringPtr> names = ...;
auto list = person.initNames(names.size());
for (size_t i = 0; i < names.size(); i++) {
  list.set(i, names[i]);
}`}</code></pre>

      <h2>Reading Messages</h2>

      <h3>Reader Interface</h3>

      <pre><code>{`#include <capnp/message.h>
#include <capnp/serialize.h>
#include "person.capnp.h"

// Read from file descriptor
capnp::StreamFdMessageReader message(fd);
auto person = message.getRoot<Person>();

// Access fields
kj::StringPtr name = person.getName();
uint32_t age = person.getAge();

// Iterate over lists
for (auto phone : person.getPhones()) {
  kj::StringPtr number = phone.getNumber();
  auto type = phone.getType();
}`}</code></pre>

      <h3>Reader Methods</h3>

      <table>
        <thead>
          <tr>
            <th>Method</th>
            <th>Description</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><code>getField()</code></td>
            <td>Get field value (returns default if not set)</td>
          </tr>
          <tr>
            <td><code>hasField()</code></td>
            <td>Check if pointer field is set</td>
          </tr>
          <tr>
            <td><code>isField()</code></td>
            <td>Check which union member is set</td>
          </tr>
          <tr>
            <td><code>which()</code></td>
            <td>Get the active union member</td>
          </tr>
        </tbody>
      </table>

      <h2>Serialization</h2>

      <h3>Standard Format</h3>

      <pre><code>{`#include <capnp/serialize.h>

// Write to file descriptor
capnp::writeMessageToFd(fd, message);

// Write to output stream
kj::FdOutputStream output(fd);
capnp::writeMessage(output, message);

// Read from file descriptor
capnp::StreamFdMessageReader reader(fd);

// Read from input stream
kj::FdInputStream input(fd);
capnp::InputStreamMessageReader reader(input);`}</code></pre>

      <h3>Packed Format</h3>

      <p>Packed format reduces size by compressing zero bytes:</p>

      <pre><code>{`#include <capnp/serialize-packed.h>

// Write packed
capnp::writePackedMessageToFd(fd, message);

// Read packed
capnp::PackedFdMessageReader reader(fd);`}</code></pre>

      <h3>Flat Arrays</h3>

      <pre><code>{`#include <capnp/serialize.h>

// Get as flat array
kj::Array<capnp::word> words = capnp::messageToFlatArray(message);

// Read from flat array
capnp::FlatArrayMessageReader reader(words);`}</code></pre>

      <h2>Unions</h2>

      <h3>Building Unions</h3>

      <pre><code>{`// Schema:
// struct Shape {
//   union {
//     circle @0 :Circle;
//     rectangle @1 :Rectangle;
//   }
// }

auto shape = message.initRoot<Shape>();

// Set circle variant
auto circle = shape.initCircle();
circle.setRadius(5.0);

// Or set rectangle variant
auto rect = shape.initRectangle();
rect.setWidth(10.0);
rect.setHeight(20.0);`}</code></pre>

      <h3>Reading Unions</h3>

      <pre><code>{`auto shape = reader.getRoot<Shape>();

switch (shape.which()) {
  case Shape::CIRCLE: {
    auto circle = shape.getCircle();
    double r = circle.getRadius();
    break;
  }
  case Shape::RECTANGLE: {
    auto rect = shape.getRectangle();
    double w = rect.getWidth();
    double h = rect.getHeight();
    break;
  }
}`}</code></pre>

      <h2>Orphans</h2>

      <p>Orphans are objects not yet attached to a message:</p>

      <pre><code>{`// Create an orphan
auto orphan = message.getOrphanage().newOrphan<Person>();
auto person = orphan.get();
person.setName("Alice");

// Adopt into a field
parent.adoptChild(kj::mv(orphan));

// Disown from a field
auto orphan = parent.disownChild();`}</code></pre>

      <h2>Dynamic API</h2>

      <p>Access messages without compile-time schema:</p>

      <pre><code>{`#include <capnp/dynamic.h>
#include <capnp/schema-loader.h>

// Load schema at runtime
capnp::SchemaLoader loader;
capnp::Schema schema = loader.loadOnce(schemaBytes);

// Read dynamically
capnp::DynamicStruct::Reader dynamic = reader.getRoot<capnp::DynamicStruct>(schema);

// Access fields by name
auto name = dynamic.get("name").as<capnp::Text>();
auto age = dynamic.get("age").as<uint32_t>();`}</code></pre>

      <h2>Traversal Limits</h2>

      <p>Protect against malicious messages:</p>

      <pre><code>{`capnp::ReaderOptions options;
options.traversalLimitInWords = 64 * 1024 * 1024;  // 512 MB
options.nestingLimit = 64;

capnp::StreamFdMessageReader reader(fd, options);`}</code></pre>

      <h2>Memory Management</h2>

      <h3>Arena Allocation</h3>

      <pre><code>{`// Pre-allocate a buffer
kj::byte buffer[4096];
kj::ArrayPtr<kj::byte> scratch(buffer, sizeof(buffer));

// Use scratch space for message
capnp::MallocMessageBuilder message(
    capnp::SUGGESTED_FIRST_SEGMENT_WORDS,
    capnp::AllocationStrategy::FIXED_SIZE,
    scratch);`}</code></pre>

      <h3>Copying Messages</h3>

      <pre><code>{`// Deep copy a message
capnp::MallocMessageBuilder copy;
copy.setRoot(reader.getRoot<Person>());`}</code></pre>

      <h2>Error Handling</h2>

      <pre><code>{`#include <kj/exception.h>

try {
  capnp::StreamFdMessageReader reader(fd);
  auto root = reader.getRoot<Person>();
  // ... process message
} catch (const kj::Exception& e) {
  KJ_LOG(ERROR, "Failed to read message", e.getDescription());
}`}</code></pre>

      <h2>Thread Safety</h2>

      <ul>
        <li>MessageBuilder is not thread-safe for concurrent writes</li>
        <li>MessageReader is safe for concurrent reads</li>
        <li>Multiple readers can share the same message data</li>
      </ul>

      <h2>Next Steps</h2>

      <ul>
        <li>Learn about the <a href="/docs/kj">KJ library</a></li>
        <li>Explore the <a href="/docs/rpc">RPC system</a></li>
        <li>See <a href="/docs/examples">examples</a></li>
      </ul>
    </DocsLayout>
  )
}
