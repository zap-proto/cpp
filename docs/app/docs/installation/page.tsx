import { DocsLayout } from '@/components/docs-layout'

export default function Installation() {
  return (
    <DocsLayout>
      <h1>Installation</h1>

      <p>
        This guide covers installing ZAP C++ on various platforms.
      </p>

      <h2>System Requirements</h2>

      <ul>
        <li>C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)</li>
        <li>CMake 3.16 or later</li>
        <li>pkg-config (optional, for system integration)</li>
      </ul>

      <h2>From Source</h2>

      <p>The recommended way to install ZAP C++ is from source:</p>

      <pre><code>{`# Clone the repository
git clone https://github.com/zap-protocol/zap-cpp.git
cd zap-cpp

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

# Install (may require sudo)
cmake --install .`}</code></pre>

      <h2>CMake Options</h2>

      <table>
        <thead>
          <tr>
            <th>Option</th>
            <th>Default</th>
            <th>Description</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td><code>BUILD_TESTING</code></td>
            <td>ON</td>
            <td>Build test suite</td>
          </tr>
          <tr>
            <td><code>BUILD_SHARED_LIBS</code></td>
            <td>OFF</td>
            <td>Build shared libraries</td>
          </tr>
          <tr>
            <td><code>CAPNP_LITE</code></td>
            <td>OFF</td>
            <td>Build lite version (no RPC, reflection)</td>
          </tr>
          <tr>
            <td><code>WITH_OPENSSL</code></td>
            <td>ON</td>
            <td>Enable TLS support via OpenSSL</td>
          </tr>
          <tr>
            <td><code>WITH_ZLIB</code></td>
            <td>ON</td>
            <td>Enable compression support</td>
          </tr>
        </tbody>
      </table>

      <h3>Example: Minimal Build</h3>

      <pre><code>{`cmake .. \\
  -DCMAKE_BUILD_TYPE=Release \\
  -DBUILD_TESTING=OFF \\
  -DCAPNP_LITE=ON`}</code></pre>

      <h2>Package Managers</h2>

      <h3>macOS (Homebrew)</h3>

      <pre><code>{`# Install from tap
brew tap zap-protocol/tap
brew install zap-cpp`}</code></pre>

      <h3>Ubuntu/Debian</h3>

      <pre><code>{`# Add ZAP repository
curl -fsSL https://pkg.zap-protocol.org/gpg | sudo gpg --dearmor -o /usr/share/keyrings/zap.gpg
echo "deb [signed-by=/usr/share/keyrings/zap.gpg] https://pkg.zap-protocol.org/apt stable main" | \\
  sudo tee /etc/apt/sources.list.d/zap.list

# Install
sudo apt update
sudo apt install zap-cpp-dev`}</code></pre>

      <h3>Arch Linux</h3>

      <pre><code>{`# Install from AUR
yay -S zap-cpp`}</code></pre>

      <h2>vcpkg</h2>

      <pre><code>{`# Add to vcpkg.json
{
  "dependencies": ["zap-cpp"]
}

# Or install directly
vcpkg install zap-cpp`}</code></pre>

      <h2>Conan</h2>

      <pre><code>{`# Add to conanfile.txt
[requires]
zap-cpp/1.0.0

# Or install directly
conan install zap-cpp/1.0.0@`}</code></pre>

      <h2>Verifying Installation</h2>

      <p>After installation, verify that the tools are available:</p>

      <pre><code>{`# Check compiler version
capnp --version

# Compile a test schema
echo "@0x85150b117366d14b; struct Test { value @0 :Int32; }" > test.capnp
capnp compile -oc++ test.capnp

# Verify generated files
ls test.capnp.h test.capnp.c++`}</code></pre>

      <h2>Docker</h2>

      <p>A Docker image with ZAP C++ pre-installed is available:</p>

      <pre><code>{`# Pull the image
docker pull ghcr.io/zap-protocol/zap-cpp:latest

# Run with your project mounted
docker run -it -v $(pwd):/workspace ghcr.io/zap-protocol/zap-cpp:latest

# Inside container
cd /workspace
capnp compile -oc++ schema.capnp`}</code></pre>

      <h2>Troubleshooting</h2>

      <h3>CMake cannot find capnp</h3>

      <p>If CMake cannot find the installed library:</p>

      <pre><code>{`# Set the prefix path
cmake .. -DCMAKE_PREFIX_PATH=/usr/local

# Or use pkg-config
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH`}</code></pre>

      <h3>Linker errors with shared libraries</h3>

      <p>Update the library path:</p>

      <pre><code>{`# Linux
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
sudo ldconfig

# macOS
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH`}</code></pre>

      <h3>Compiler version issues</h3>

      <p>Ensure you have a C++20 compatible compiler:</p>

      <pre><code>{`# Check GCC version
g++ --version  # Should be 10+

# Check Clang version
clang++ --version  # Should be 12+

# Specify compiler for CMake
cmake .. -DCMAKE_CXX_COMPILER=g++-12`}</code></pre>

      <h2>Next Steps</h2>

      <ul>
        <li>Learn how to <a href="/docs/building">build projects</a> with ZAP C++</li>
        <li>Understand the <a href="/docs/schema">schema language</a></li>
      </ul>
    </DocsLayout>
  )
}
