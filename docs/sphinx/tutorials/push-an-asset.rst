Build a server that pushes an asset alongside the response
==========================================================

**You'll build:** an HTTP/2 server that, when asked for ``/``, responds
with a small HTML document *and* proactively pushes ``/app.css`` so the
browser has the stylesheet before the parser finds the ``<link>`` tag.
Server push is the feature HTTP/2 added to shave one RTT off the first
render; it lives or dies on correctness, so we'll wire it up carefully.

**You'll use:**
:cpp:func:`createServer <polycpp::http2::createServer>`,
:cpp:class:`ServerHttp2Stream <polycpp::http2::ServerHttp2Stream>`,
:cpp:func:`ServerHttp2Stream::pushStream <polycpp::http2::ServerHttp2Stream::pushStream>`,
:cpp:func:`ServerHttp2Stream::respond <polycpp::http2::ServerHttp2Stream::respond>`.

Step 1 — route by ``:path``
---------------------------

.. code-block:: cpp

   #include <polycpp/http2/http2.hpp>
   #include <polycpp/io/event_context.hpp>
   using namespace polycpp;

   auto onStream = [](http2::ServerHttp2Stream stream, polycpp::http::Headers hdrs) {
       const auto path = hdrs[":path"];
       if (path == "/") {
           serveHome(stream);           // step 2
       } else if (path == "/app.css") {
           serveCss(stream);
       } else {
           stream.respond(404, {{"content-type", "text/plain"}});
           stream.end("not found\n");
       }
   };

   EventContext ctx;
   auto server = http2::createServer(ctx, {}, onStream);
   server.listen(8080);
   ctx.run();

Step 2 — push ``/app.css`` before sending the HTML
--------------------------------------------------

Push on the main stream; the callback gives you a
:cpp:class:`ServerHttp2Stream <polycpp::http2::ServerHttp2Stream>`
that's already associated with the ``/app.css`` path:

.. code-block:: cpp

   void serveHome(http2::ServerHttp2Stream stream) {
       // 1. Promise the asset.
       stream.pushStream(
           polycpp::http::Headers{
               {":method",    "GET"},
               {":scheme",    "http"},
               {":authority", "localhost:8080"},
               {":path",      "/app.css"},
           },
           [](const polycpp::Error* err, http2::ServerHttp2Stream push, const polycpp::http::Headers&) {
               if (err) return;                      // client refused the push
               push.respond(200, {{"content-type", "text/css"}});
               push.end("body { color: #222 }\n");
           });

       // 2. Send the HTML that references the asset we just promised.
       stream.respond(200, {{"content-type", "text/html"}});
       stream.end(
           "<!doctype html>"
           "<link rel='stylesheet' href='/app.css'>"
           "<h1>hello</h1>");
   }

Order matters: you must call
:cpp:func:`pushStream <polycpp::http2::ServerHttp2Stream::pushStream>`
*before* sending the HEADERS frame for the main response. Node
recommends this, and the C++ API carries the same constraint.

Step 3 — serve ``/app.css`` for the non-push path
-------------------------------------------------

Clients can disable push via the ``SETTINGS_ENABLE_PUSH`` setting.
Always also provide a plain route so the browser has a fallback:

.. code-block:: cpp

   void serveCss(http2::ServerHttp2Stream stream) {
       stream.respond(200, {{"content-type", "text/css"}});
       stream.end("body { color: #222 }\n");
   }

Step 4 — verify with nghttp
---------------------------

The ``nghttp`` CLI is the authoritative way to confirm pushes land:

.. code-block:: bash

   nghttp -nv http://localhost:8080/

You'll see two streams in the trace — the push (``stream id 2``) with
its HEADERS + DATA frames, and the main response on ``stream id 1``.
In browsers, DevTools shows pushed assets under their own
"Initiator: Push" badge in the Network panel.

Step 5 — when to skip push
--------------------------

Push is costly when the client already has the asset cached.
Production services typically combine:

- a short-lived cache cookie that flags "already saw the CSS";
- a ``Cache-Digest`` style hint if you support it;
- giving up and relying on ``103 Early Hints`` instead.

:cpp:func:`pushStream <polycpp::http2::ServerHttp2Stream::pushStream>`
is a tool, not a strategy — measure.

What you learned
----------------

- ``pushStream`` promises an asset; the callback gives you the stream
  the client will receive it on.
- Always provide a non-push route for the same asset — clients can
  disable push.
- Push before the main response's HEADERS, and verify with
  ``nghttp -nv``.
