// HTTP/2 push. When the client requests /index.html, the server also
// pushes /app.css on the same session. Pair with a browser that still
// accepts push (or `curl --http2-prior-knowledge -v`) to observe.
//
//   ./build/examples/push_asset

#include <polycpp/http2/http2.hpp>
#include <polycpp/io/event_context.hpp>
#include <polycpp/event_loop.hpp>

#include <iostream>
#include <string>

namespace h2 = polycpp::http2;

int main() {
    polycpp::EventContext ctx;

    auto server = h2::createServer(ctx, {}, [](auto stream, auto headers) {
        const auto path = headers.get(":path").value_or("/");

        if (path == "/index.html") {
            // Push the stylesheet alongside the HTML.
            stream.pushStream({{":method", "GET"}, {":path", "/app.css"}},
                              [](auto pushStream, auto /*pushHeaders*/) {
                                  pushStream.respond(200,
                                      {{"content-type", "text/css"}});
                                  pushStream.end("body { font-family: system-ui; }\n");
                              });

            stream.respond(200, {{"content-type", "text/html"}});
            stream.end("<!doctype html><link rel=stylesheet href=app.css><h1>Hi</h1>");
            return;
        }

        // Fallback — 404 for any other path.
        stream.respond(404, {{"content-type", "text/plain"}});
        stream.end("not found");
    });

    server.listen(8080);
    std::cout << "HTTP/2 push example listening on :8080\n";
    polycpp::event_loop::run();
}
