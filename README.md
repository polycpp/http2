# polycpp/http2

HTTP/2 client and server support for the polycpp ecosystem using nghttp2.

## Prerequisites

- CMake 3.20+
- GCC 13+ or Clang 16+
- C++20 support
- Access to the base polycpp library

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPOLYCPP_HTTP2_BUILD_TESTS=ON
cmake --build build -j$(nproc)
```

For local development against an adjacent polycpp checkout, pass:

```bash
-DPOLYCPP_SOURCE_DIR=/path/to/polycpp
```

## Running Tests

```bash
cd build && ctest --output-on-failure
```

## License

MIT License. See [LICENSE](LICENSE) for details.
