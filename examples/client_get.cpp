// HTTP/2 client: issues a single GET and prints the response.
//
//   ./build/examples/client_get http://localhost:8080/

#include <polycpp/http2/http2.hpp>
#include <polycpp/io/event_context.hpp>
#include <polycpp/event_loop.hpp>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: client_get <url>\n";
        return 2;
    }
    const std::string url = argv[1];

    polycpp::EventContext ctx;
    auto session = polycpp::http2::connect(ctx, url);

    int exitCode = 0;
    auto stream = session.request({{":method", "GET"}, {":path", "/"}});

    stream.on(polycpp::http2::event::Response,
              [&exitCode](const polycpp::http::Headers& headers) {
                  auto status = headers.get(":status").value_or("0");
                  std::cout << "HTTP/2 " << status << '\n';
                  exitCode = std::stoi(status);
              });

    stream.on(polycpp::http2::event::Data,
              [](const polycpp::buffer::Buffer& chunk) {
                  std::cout.write(reinterpret_cast<const char*>(chunk.data()),
                                  static_cast<std::streamsize>(chunk.size()));
              });

    stream.on(polycpp::http2::event::End,
              [&session]() { session.close(); });

    polycpp::event_loop::run();
    return exitCode >= 400 ? 1 : 0;
}
