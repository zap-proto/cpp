import { DocsLayout } from '@/components/docs-layout'

export default function Serialization() {
  return (
    <DocsLayout>
      <h1>Serialization</h1>

      <p>
        ZAP C++ provides zero-copy serialization, meaning data can be read directly
        from the wire without parsing or copying.
      </p>

      <h2>Wire Format</h2>

      <p>
        The Cap'n Proto wire format stores data in a way that maps directly to
        in-memory structures. Messages consist of:
      </p>

      <ul>
        <li><strong>Segments</strong> - Contiguous blocks of memory</li>
        <li><strong>Pointers</strong> - References between objects</li>
        <li><strong>Data</strong> - Primitive values and blob data</li>
      </ul>

      <h3>Segment Structure</h3>

      <pre><code>{`┌─────────────────────────────────────┐
│ Segment Table (4 + 4*N bytes)       │
│  - segment count (4 bytes)          │
│  - segment 0 size (4 bytes)         │
│  - segment 1 size (4 bytes)         │
│  ...                                │
├─────────────────────────────────────┤
│ Segment 0 data                      │
├─────────────────────────────────────┤
│ Segment 1 data                      │
├─────────────────────────────────────┤
│ ...                                 │
└─────────────────────────────────────┘`}</code></pre>

      <h2>Serialization Methods</h2>

      <h3>Standard Serialization</h3>

      <pre><code>{`#include <capnp/message.h>
#include <capnp/serialize.h>

// Build message
capnp::MallocMessageBuilder message;
auto root = message.initRoot<MyStruct>();
root.setField("value");

// Serialize to file descriptor
capnp::writeMessageToFd(fd, message);

// Serialize to array
kj::Array<capnp::word> words = capnp::messageToFlatArray(message);

// Serialize to vector
kj::VectorOutputStream output;
capnp::writeMessage(output, message);
auto bytes = output.getArray();`}</code></pre>

      <h3>Packed Serialization</h3>

      <p>Packed format compresses zero bytes for smaller messages:</p>

      <pre><code>{`#include <capnp/serialize-packed.h>

// Write packed
capnp::writePackedMessageToFd(fd, message);

// Write packed to stream
kj::FdOutputStream output(fd);
capnp::writePackedMessage(output, message);

// Read packed
capnp::PackedFdMessageReader reader(fd);`}</code></pre>

      <h3>Compression Comparison</h3>

      <table>
        <thead>
          <tr>
            <th>Format</th>
            <th>Size</th>
            <th>Speed</th>
            <th>Use Case</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>Standard</td>
            <td>Larger</td>
            <td>Fastest</td>
            <td>Local IPC, memory-mapped files</td>
          </tr>
          <tr>
            <td>Packed</td>
            <td>Smaller</td>
            <td>Fast</td>
            <td>Network, storage</td>
          </tr>
        </tbody>
      </table>

      <h2>Deserialization</h2>

      <h3>From File Descriptor</h3>

      <pre><code>{`#include <capnp/serialize.h>

// Standard format
capnp::StreamFdMessageReader reader(fd);
auto root = reader.getRoot<MyStruct>();

// Packed format
capnp::PackedFdMessageReader packedReader(fd);
auto root2 = packedReader.getRoot<MyStruct>();`}</code></pre>

      <h3>From Byte Array</h3>

      <pre><code>{`// From flat array (zero-copy if properly aligned)
kj::ArrayPtr<const capnp::word> words(
    reinterpret_cast<const capnp::word*>(data),
    size / sizeof(capnp::word));
capnp::FlatArrayMessageReader reader(words);

// From unaligned data
capnp::UnalignedFlatArrayMessageReader reader(
    kj::arrayPtr(data, size));`}</code></pre>

      <h3>From Input Stream</h3>

      <pre><code>{`kj::FdInputStream input(fd);
capnp::InputStreamMessageReader reader(input);

// With options
capnp::ReaderOptions options;
options.traversalLimitInWords = 128 * 1024 * 1024;
capnp::InputStreamMessageReader reader(input, options);`}</code></pre>

      <h2>Memory Mapping</h2>

      <p>For maximum performance with files, use memory mapping:</p>

      <pre><code>{`#include <sys/mman.h>
#include <capnp/serialize.h>

// Map file into memory
int fd = open("data.capnp", O_RDONLY);
struct stat st;
fstat(fd, &st);
void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

// Read directly from mapped memory
kj::ArrayPtr<const capnp::word> words(
    reinterpret_cast<const capnp::word*>(data),
    st.st_size / sizeof(capnp::word));
capnp::FlatArrayMessageReader reader(words);

auto root = reader.getRoot<MyStruct>();

// Data is accessed directly from the mmap - no copying!`}</code></pre>

      <h2>Streaming Multiple Messages</h2>

      <h3>Writing</h3>

      <pre><code>{`kj::FdOutputStream output(fd);

for (auto& item : items) {
  capnp::MallocMessageBuilder message;
  auto root = message.initRoot<Item>();
  root.setName(item.name);
  root.setValue(item.value);

  capnp::writeMessage(output, message);
}`}</code></pre>

      <h3>Reading</h3>

      <pre><code>{`kj::FdInputStream input(fd);

while (true) {
  kj::Maybe<capnp::InputStreamMessageReader> maybeReader;
  KJ_IF_MAYBE(reader, capnp::tryReadMessage(input)) {
    auto root = reader->getRoot<Item>();
    process(root);
  } else {
    break;  // End of stream
  }
}`}</code></pre>

      <h2>Security Considerations</h2>

      <h3>Traversal Limits</h3>

      <p>Protect against malicious messages that could cause excessive memory use:</p>

      <pre><code>{`capnp::ReaderOptions options;

// Limit total words traversed (default: 8 * 1024 * 1024)
options.traversalLimitInWords = 64 * 1024 * 1024;

// Limit nesting depth (default: 64)
options.nestingLimit = 128;

capnp::StreamFdMessageReader reader(fd, options);`}</code></pre>

      <h3>Untrusted Input</h3>

      <pre><code>{`// Always use try/catch for untrusted input
try {
  capnp::StreamFdMessageReader reader(fd, options);
  auto root = reader.getRoot<Message>();

  // Validate before use
  if (!root.hasRequiredField()) {
    throw std::runtime_error("Missing required field");
  }

  process(root);
} catch (const kj::Exception& e) {
  // Handle malformed message
  log("Invalid message: ", e.getDescription());
}`}</code></pre>

      <h2>Text Format</h2>

      <p>For debugging and configuration, use the text format:</p>

      <pre><code>{`#include <capnp/pretty-print.h>
#include <capnp/serialize-text.h>

// Print as text
auto text = capnp::prettyPrint(root).flatten();
std::cout << text.cStr() << std::endl;

// Output:
// ( name = "Alice",
//   age = 30,
//   phones = [
//     ( number = "+1-555-1234", type = mobile ),
//     ( number = "+1-555-5678", type = work )
//   ] )`}</code></pre>

      <h2>Performance Tips</h2>

      <ol>
        <li>
          <strong>Reuse MessageBuilder</strong> - Call <code>message.clear()</code>
          instead of creating new builders
        </li>
        <li>
          <strong>Use packed format for network</strong> - Reduces bandwidth at
          minimal CPU cost
        </li>
        <li>
          <strong>Memory map large files</strong> - Avoids copying data entirely
        </li>
        <li>
          <strong>Pre-size lists</strong> - Use <code>initList(size)</code> to
          avoid reallocations
        </li>
        <li>
          <strong>Avoid dynamic API for hot paths</strong> - Compile-time types
          are faster
        </li>
      </ol>

      <h2>Next Steps</h2>

      <ul>
        <li>Learn about the <a href="/docs/rpc">RPC system</a></li>
        <li>See <a href="/docs/examples">examples</a></li>
      </ul>
    </DocsLayout>
  )
}
