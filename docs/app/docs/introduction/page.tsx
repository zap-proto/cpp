import { DocsLayout } from '@/components/docs-layout'

export default function Introduction() {
  return (
    <DocsLayout>
      <h1>Introduction</h1>

      <p>
        ZAP C++ is the ZAP Protocol's implementation of Cap'n Proto, a data
        serialization and RPC framework focused on performance and usability.
      </p>

      <h2>Why ZAP C++?</h2>

      <p>
        Traditional serialization formats like JSON, XML, or even Protocol Buffers
        require encoding data into bytes and then decoding it back. This process
        takes time and allocates memory.
      </p>

      <p>
        ZAP C++ takes a different approach: data is stored in a format that can be
        directly used without parsing. When you receive a message, you can immediately
        access any field without first scanning through the entire message.
      </p>

      <h2>Key Advantages</h2>

      <h3>Zero-Copy Design</h3>
      <p>
        Messages can be memory-mapped directly from disk or network buffers.
        No copying or parsing required. This makes ZAP C++ ideal for:
      </p>
      <ul>
        <li>Memory-mapped databases</li>
        <li>High-frequency trading systems</li>
        <li>Real-time communication</li>
        <li>Embedded systems with limited resources</li>
      </ul>

      <h3>Schema Evolution</h3>
      <p>
        ZAP C++ schemas can evolve over time while maintaining both forward and
        backward compatibility:
      </p>
      <ul>
        <li>New fields can be added to any struct</li>
        <li>Old code can read new messages (ignoring new fields)</li>
        <li>New code can read old messages (using defaults for missing fields)</li>
      </ul>

      <h3>Capability-Based Security</h3>
      <p>
        The RPC system implements object-capability security. Instead of checking
        permissions at every operation, capabilities (references to objects) are
        passed around. If you have a reference, you have permission to use it.
      </p>

      <h3>Promise Pipelining</h3>
      <p>
        When making an RPC call that returns an object, you can immediately make
        calls on that object without waiting for the first call to complete.
        The system automatically pipelines these requests, reducing latency.
      </p>

      <h2>Architecture Overview</h2>

      <p>ZAP C++ consists of several components:</p>

      <table>
        <thead>
          <tr>
            <th>Component</th>
            <th>Description</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><code>capnp</code></td>
            <td>Core serialization library</td>
          </tr>
          <tr>
            <td><code>capnpc</code></td>
            <td>Schema compiler</td>
          </tr>
          <tr>
            <td><code>capnp-rpc</code></td>
            <td>RPC implementation</td>
          </tr>
          <tr>
            <td><code>kj</code></td>
            <td>Utility library (async I/O, strings, etc.)</td>
          </tr>
          <tr>
            <td><code>kj-async</code></td>
            <td>Asynchronous I/O and promises</td>
          </tr>
          <tr>
            <td><code>kj-http</code></td>
            <td>HTTP client/server</td>
          </tr>
          <tr>
            <td><code>kj-tls</code></td>
            <td>TLS support</td>
          </tr>
        </tbody>
      </table>

      <h2>Comparison with Other Formats</h2>

      <table>
        <thead>
          <tr>
            <th>Feature</th>
            <th>ZAP C++</th>
            <th>Protocol Buffers</th>
            <th>JSON</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>Zero-copy</td>
            <td>Yes</td>
            <td>No</td>
            <td>No</td>
          </tr>
          <tr>
            <td>Schema required</td>
            <td>Yes</td>
            <td>Yes</td>
            <td>No</td>
          </tr>
          <tr>
            <td>Human readable</td>
            <td>No*</td>
            <td>No</td>
            <td>Yes</td>
          </tr>
          <tr>
            <td>RPC support</td>
            <td>Built-in</td>
            <td>gRPC (separate)</td>
            <td>No</td>
          </tr>
          <tr>
            <td>Promise pipelining</td>
            <td>Yes</td>
            <td>No</td>
            <td>No</td>
          </tr>
        </tbody>
      </table>

      <p>
        <em>* ZAP C++ includes a text format for debugging and configuration files.</em>
      </p>

      <h2>Next Steps</h2>

      <ul>
        <li>Follow the <a href="/docs/installation">Installation Guide</a></li>
        <li>Learn the <a href="/docs/schema">Schema Language</a></li>
        <li>Explore the <a href="/docs/examples">Examples</a></li>
      </ul>
    </DocsLayout>
  )
}
