import Link from 'next/link'
import { DocsLayout } from '@/components/docs-layout'

export default function Home() {
  return (
    <DocsLayout>
      <h1>ZAP C++ Documentation</h1>

      <p>
        Welcome to the documentation for <strong>ZAP C++</strong>, the ZAP Protocol's
        fork of Cap'n Proto - an insanely fast data interchange format and
        capability-based RPC system.
      </p>

      <h2>What is ZAP C++?</h2>

      <p>
        ZAP C++ is a high-performance serialization and RPC library forked from
        Cap'n Proto. It provides:
      </p>

      <ul>
        <li><strong>Zero-copy serialization</strong> - No encoding/decoding step required</li>
        <li><strong>Schema evolution</strong> - Add fields without breaking compatibility</li>
        <li><strong>Capability-based RPC</strong> - Secure, object-capability model</li>
        <li><strong>Promise pipelining</strong> - Reduce round-trip latency</li>
      </ul>

      <h2>Quick Start</h2>

      <p>Get started with ZAP C++ in three steps:</p>

      <ol>
        <li>
          <Link href="/docs/installation">Install</Link> the library and compiler
        </li>
        <li>
          <Link href="/docs/schema">Define your schema</Link> using the Cap'n Proto language
        </li>
        <li>
          <Link href="/docs/examples">Build your application</Link> using the generated C++ code
        </li>
      </ol>

      <h2>Features</h2>

      <h3>Serialization</h3>
      <p>
        ZAP C++ serialization is faster than Protocol Buffers because there is no
        encoding/decoding step. Data is stored in a format that can be directly
        memory-mapped and accessed without parsing.
      </p>

      <h3>RPC System</h3>
      <p>
        The RPC system implements the object-capability security model with
        promise pipelining. This allows you to make multiple dependent calls
        in a single round trip.
      </p>

      <h3>KJ Library</h3>
      <p>
        ZAP C++ includes KJ, a modern C++ utility library providing async I/O,
        string utilities, containers, and more. KJ is designed for C++20 and
        emphasizes safety and performance.
      </p>

      <h2>Documentation Sections</h2>

      <ul>
        <li><Link href="/docs/introduction">Introduction</Link> - Overview and concepts</li>
        <li><Link href="/docs/installation">Installation</Link> - System requirements and setup</li>
        <li><Link href="/docs/building">Building</Link> - CMake configuration and compilation</li>
        <li><Link href="/docs/schema">Schema Language</Link> - Defining data structures</li>
        <li><Link href="/docs/api">API Reference</Link> - C++ API documentation</li>
        <li><Link href="/docs/examples">Examples</Link> - Code samples and tutorials</li>
      </ul>
    </DocsLayout>
  )
}
