import { DocsLayout } from '@/components/docs-layout'

export default function Schema() {
  return (
    <DocsLayout>
      <h1>Schema Language</h1>

      <p>
        The Cap'n Proto schema language defines the structure of your messages.
        Schemas are compiled to generate type-safe code for your target language.
      </p>

      <h2>File Structure</h2>

      <p>Every schema file must start with a unique ID:</p>

      <pre><code>{`# Generate a unique ID
capnp id

# Use it in your schema
@0x85150b117366d14b;`}</code></pre>

      <h2>Primitive Types</h2>

      <table>
        <thead>
          <tr>
            <th>Type</th>
            <th>Description</th>
            <th>C++ Type</th>
          </tr>
        </thead>
        <tbody>
          <tr><td><code>Void</code></td><td>No data</td><td><code>capnp::Void</code></td></tr>
          <tr><td><code>Bool</code></td><td>Boolean</td><td><code>bool</code></td></tr>
          <tr><td><code>Int8</code></td><td>Signed 8-bit</td><td><code>int8_t</code></td></tr>
          <tr><td><code>Int16</code></td><td>Signed 16-bit</td><td><code>int16_t</code></td></tr>
          <tr><td><code>Int32</code></td><td>Signed 32-bit</td><td><code>int32_t</code></td></tr>
          <tr><td><code>Int64</code></td><td>Signed 64-bit</td><td><code>int64_t</code></td></tr>
          <tr><td><code>UInt8</code></td><td>Unsigned 8-bit</td><td><code>uint8_t</code></td></tr>
          <tr><td><code>UInt16</code></td><td>Unsigned 16-bit</td><td><code>uint16_t</code></td></tr>
          <tr><td><code>UInt32</code></td><td>Unsigned 32-bit</td><td><code>uint32_t</code></td></tr>
          <tr><td><code>UInt64</code></td><td>Unsigned 64-bit</td><td><code>uint64_t</code></td></tr>
          <tr><td><code>Float32</code></td><td>32-bit float</td><td><code>float</code></td></tr>
          <tr><td><code>Float64</code></td><td>64-bit float</td><td><code>double</code></td></tr>
          <tr><td><code>Text</code></td><td>UTF-8 string</td><td><code>capnp::Text</code></td></tr>
          <tr><td><code>Data</code></td><td>Byte array</td><td><code>capnp::Data</code></td></tr>
        </tbody>
      </table>

      <h2>Structs</h2>

      <p>Structs are the primary composite type:</p>

      <pre><code>{`struct Person {
  name @0 :Text;
  birthdate @1 :Date;
  email @2 :Text;
  phones @3 :List(PhoneNumber);
}

struct Date {
  year @0 :Int16;
  month @1 :UInt8;
  day @2 :UInt8;
}

struct PhoneNumber {
  number @0 :Text;
  type @1 :Type;

  enum Type {
    mobile @0;
    home @1;
    work @2;
  }
}`}</code></pre>

      <h3>Field Numbers</h3>

      <p>
        Each field has a number (<code>@0</code>, <code>@1</code>, etc.) that identifies it
        in the binary format. These numbers:
      </p>

      <ul>
        <li>Must be unique within a struct</li>
        <li>Must start at 0 and be sequential</li>
        <li>Cannot be reused even after a field is removed</li>
      </ul>

      <h3>Default Values</h3>

      <pre><code>{`struct Config {
  timeout @0 :UInt32 = 30;
  retries @1 :UInt8 = 3;
  host @2 :Text = "localhost";
  enabled @3 :Bool = true;
}`}</code></pre>

      <h2>Enums</h2>

      <pre><code>{`enum Color {
  red @0;
  green @1;
  blue @2;
}

struct Pixel {
  x @0 :UInt16;
  y @1 :UInt16;
  color @2 :Color;
}`}</code></pre>

      <h2>Lists</h2>

      <pre><code>{`struct Document {
  title @0 :Text;
  pages @1 :List(Page);
  tags @2 :List(Text);
  scores @3 :List(Float64);
}

struct Page {
  content @0 :Text;
}`}</code></pre>

      <h2>Unions</h2>

      <p>Unions allow a field to be one of several types:</p>

      <pre><code>{`struct Shape {
  union {
    circle @0 :Circle;
    rectangle @1 :Rectangle;
    triangle @2 :Triangle;
  }
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
}`}</code></pre>

      <h3>Named Unions</h3>

      <pre><code>{`struct Value {
  name @0 :Text;

  data :union {
    intValue @1 :Int64;
    floatValue @2 :Float64;
    textValue @3 :Text;
    boolValue @4 :Bool;
  }
}`}</code></pre>

      <h2>Groups</h2>

      <p>Groups organize fields without adding a pointer indirection:</p>

      <pre><code>{`struct Person {
  name @0 :Text;

  address :group {
    street @1 :Text;
    city @2 :Text;
    zipCode @3 :Text;
    country @4 :Text;
  }
}`}</code></pre>

      <h2>Interfaces (RPC)</h2>

      <p>Interfaces define RPC methods:</p>

      <pre><code>{`interface Calculator {
  add @0 (a :Int32, b :Int32) -> (result :Int32);
  subtract @1 (a :Int32, b :Int32) -> (result :Int32);
  multiply @2 (a :Int32, b :Int32) -> (result :Int32);
  divide @3 (a :Int32, b :Int32) -> (result :Int32, remainder :Int32);
}

interface Database {
  get @0 (key :Text) -> (value :Data);
  put @1 (key :Text, value :Data) -> ();
  delete @2 (key :Text) -> ();
  list @3 (prefix :Text) -> (keys :List(Text));
}`}</code></pre>

      <h3>Returning Capabilities</h3>

      <pre><code>{`interface Session {
  login @0 (username :Text, password :Text) -> (user :User);
}

interface User {
  getName @0 () -> (name :Text);
  getProfile @1 () -> (profile :Profile);
  logout @2 () -> ();
}

struct Profile {
  email @0 :Text;
  avatar @1 :Data;
}`}</code></pre>

      <h2>Generics</h2>

      <pre><code>{`struct Map(Key, Value) {
  entries @0 :List(Entry);

  struct Entry {
    key @0 :Key;
    value @1 :Value;
  }
}

struct StringIntMap {
  data @0 :Map(Text, Int64);
}`}</code></pre>

      <h2>Imports</h2>

      <pre><code>{`# common.capnp
@0x85150b117366d14b;

struct Timestamp {
  seconds @0 :Int64;
  nanos @1 :UInt32;
}

# main.capnp
@0xc8b1a9f4e2d3b6a7;

using Common = import "common.capnp";

struct Event {
  name @0 :Text;
  timestamp @1 :Common.Timestamp;
}`}</code></pre>

      <h2>Annotations</h2>

      <pre><code>{`annotation deprecated(field, struct, enum) :Text;

struct OldApi {
  newField @0 :Text;
  oldField @1 :Text $deprecated("Use newField instead");
}`}</code></pre>

      <h2>Constants</h2>

      <pre><code>{`const maxSize :UInt32 = 1048576;
const defaultHost :Text = "localhost";
const defaultPorts :List(UInt16) = [80, 443, 8080];`}</code></pre>

      <h2>Schema Evolution</h2>

      <h3>Safe Changes</h3>
      <ul>
        <li>Adding new fields (with new numbers)</li>
        <li>Renaming fields or types</li>
        <li>Adding new enum values at the end</li>
        <li>Adding new union members</li>
        <li>Adding new methods to interfaces</li>
      </ul>

      <h3>Breaking Changes</h3>
      <ul>
        <li>Removing fields</li>
        <li>Changing field types</li>
        <li>Reusing field numbers</li>
        <li>Changing field numbers</li>
        <li>Removing enum values</li>
      </ul>

      <h2>Next Steps</h2>

      <ul>
        <li>Learn about <a href="/docs/serialization">serialization</a></li>
        <li>Explore the <a href="/docs/rpc">RPC system</a></li>
        <li>See <a href="/docs/examples">examples</a></li>
      </ul>
    </DocsLayout>
  )
}
