// HTTP/2 echo server. Listens on :8080 and echoes each request body.
//
//   cmake --build build --target echo_server
//   ./build/examples/echo_server
//   curl --http2-prior-knowledge -X POST -d 'hello' http://localhost:8080/

#include <polycpp/http2/http2.hpp>
#include <polycpp/io/event_context.hpp>
#include <polycpp/event_loop.hpp>

#include <iostream>
#include <string>

int main() {
    polycpp::EventContext ctx;

    auto server = polycpp::http2::createServer(ctx, {}, [](auto stream, auto /*headers*/) {
        // Collect the request body, then respond with the same bytes.
        auto body = std::make_shared<std::string>();
        stream.on(polycpp::http2::event::Data,
                  [body](const polycpp::buffer::Buffer& chunk) {
                      body->append(reinterpret_cast<const char*>(chunk.data()),
                                   chunk.size());
                  });
        stream.on(polycpp::http2::event::End, [stream, body]() mutable {
            stream.respond(200, {{"content-type", "application/octet-stream"},
                                 {"content-length", std::to_string(body->size())}});
            stream.end(*body);
        });
    });

    server.listen(8080);
    std::cout << "HTTP/2 echo server listening on :8080\n";
    polycpp::event_loop::run();
}
