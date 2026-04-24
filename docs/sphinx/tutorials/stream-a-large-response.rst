Stream a large response in chunks
=================================

**You'll build:** an endpoint that returns a large JSON array by writing
it out in chunks rather than buffering the whole thing into a string.
The HTTP/2 flow-control hooks make this backpressure-aware — your
server won't blow its heap producing data faster than the client can
consume it.

**You'll use:**
:cpp:func:`ServerHttp2Stream::write <polycpp::http2::ServerHttp2Stream::write>`,
:cpp:func:`ServerHttp2Stream::end <polycpp::http2::ServerHttp2Stream::end>`,
the ``"drain"`` and ``"wantTrailers"`` events on
:cpp:class:`Http2Stream <polycpp::http2::Http2Stream>`.

Step 1 — headers + open-ended body
----------------------------------

.. code-block:: cpp

   #include <polycpp/http2/http2.hpp>
   #include <polycpp/io/event_context.hpp>
   using namespace polycpp;

   void serveLargeJson(http2::ServerHttp2Stream stream) {
       // Flag waitForTrailers so we can close with a Server-Timing trailer.
       http2::StreamResponseOptions opts;
       opts.waitForTrailers = true;

       stream.respond(200,
           polycpp::http::Headers{{"content-type", "application/json"}},
           opts);

       stream.write("[");
       // ... step 2 writes individual records ...
   }

Step 2 — respect backpressure
-----------------------------

Writes return the number of queued bytes. When the write buffer is full,
subscribe to ``"drain"`` and wait before producing more:

.. code-block:: cpp

   void pump(http2::ServerHttp2Stream stream, std::shared_ptr<std::size_t> idx,
             std::size_t total) {
       while (*idx < total) {
           std::string record = renderRecord(*idx);   // your data source
           if (*idx > 0) record.insert(0, ",");
           auto written = stream.write(record);
           ++(*idx);
           if (!written) {
               // Write buffer saturated — wait for drain and resume.
               stream.once("drain", [stream, idx, total]() mutable {
                   pump(stream, idx, total);
               });
               return;
           }
       }
       // All records written — close with a trailer.
       stream.write("]");
       stream.sendTrailers({{"server-timing", "db;dur=12.3"}});
   }

The ``"wantTrailers"`` event on the stream fires when
:cpp:var:`waitForTrailers <polycpp::http2::StreamResponseOptions::waitForTrailers>`
is set and the client is ready — that's the cue to call
:cpp:func:`sendTrailers <polycpp::http2::ServerHttp2Stream::sendTrailers>`.

Step 3 — wire into the handler
------------------------------

.. code-block:: cpp

   auto onStream = [](http2::ServerHttp2Stream stream, polycpp::http::Headers) {
       // ... Step 1 opens the response ...
       serveLargeJson(stream);
       auto idx = std::make_shared<std::size_t>(0);
       stream.on("wantTrailers", [stream, idx]() mutable {
           pump(stream, idx, /*total=*/100'000);
       });
   };

   EventContext ctx;
   auto server = http2::createServer(ctx, {}, onStream);
   server.listen(8080);
   ctx.run();

Step 4 — monitor flow-control windows
-------------------------------------

:cpp:func:`Http2Stream::state <polycpp::http2::Http2Stream::state>`
returns the current
:cpp:struct:`StreamState <polycpp::http2::StreamState>`, which includes
``localWindowSize``, ``remoteWindowSize``, and ``state``. Log it from a
periodic timer when diagnosing stalls — the flow-control window is
usually the first thing to investigate when HTTP/2 throughput drops
below HTTP/1.1.

What you learned
----------------

- ``write`` is synchronous but returns ``false`` when buffered — pause
  on ``"drain"``.
- Trailers let you emit end-of-body metadata (e.g., timings) without
  padding the headers.
- :cpp:struct:`StreamState <polycpp::http2::StreamState>` exposes the
  flow-control windows for debugging.
