Make a client request with custom settings
==========================================

**You'll build:** a client that connects to an HTTP/2 server, tunes the
local window size and concurrency, sends a request, collects the
response, and closes cleanly. The point is to show the full lifecycle:
:cpp:func:`connect <polycpp::http2::connect>` →
:cpp:func:`request <polycpp::http2::ClientHttp2Session::request>` →
:cpp:class:`ClientHttp2Stream <polycpp::http2::ClientHttp2Stream>` →
:cpp:func:`close <polycpp::http2::Http2Session::close>`.

**You'll use:**
:cpp:func:`connect <polycpp::http2::connect>`,
:cpp:class:`ClientSessionOptions <polycpp::http2::ClientSessionOptions>`,
:cpp:class:`Settings <polycpp::http2::Settings>`.

Step 1 — configure the session
------------------------------

.. code-block:: cpp

   #include <polycpp/http2/http2.hpp>
   #include <polycpp/io/event_context.hpp>
   using namespace polycpp;

   http2::ClientSessionOptions opts;
   opts.settings.initialWindowSize    = 1 << 20;   // 1 MiB per stream
   opts.settings.maxConcurrentStreams = 32;        // only speak to well-behaved peers
   opts.settings.enablePush           = false;     // this is a CLI, we don't want push

Every field on :cpp:struct:`Settings <polycpp::http2::Settings>` is
optional; only set fields are serialised to the wire in the client's
initial ``SETTINGS`` frame. The server's ``ACK`` completes the
negotiation; your local view is what
:cpp:func:`Http2Session::state <polycpp::http2::Http2Session::state>`
reports afterwards.

Step 2 — connect
----------------

.. code-block:: cpp

   EventContext ctx;
   auto session = http2::connect(ctx, "http://localhost:8080", opts,
       [](http2::ClientHttp2Session& s) {
           // Called once the TCP handshake finishes and the read loop is live.
       });

   session.on("error", [](const polycpp::Error& e) {
       std::cerr << "session error: " << e.message() << '\n';
   });

Step 3 — send a request
-----------------------

.. code-block:: cpp

   auto stream = session.request(polycpp::http::Headers{
       {":method",    "GET"},
       {":path",      "/"},
       {":scheme",    "http"},
       {":authority", "localhost:8080"},
   });

   std::string body;
   stream.on("response", [](const polycpp::http::Headers& hdrs, int) {
       // hdrs[":status"] holds the status code as a string.
   });
   stream.on("data", [&body](const polycpp::buffer::Buffer& chunk) {
       body.append(chunk.toString());
   });
   stream.on("end", [&]() {
       std::cout << "got " << body.size() << " bytes\n";
       session.close();   // triggers GOAWAY + socket close
   });

Step 4 — drive the loop
-----------------------

.. code-block:: cpp

   ctx.run();

``ctx.run()`` returns when there's no more work — sessions, servers,
timers, and sockets all register file-descriptor watchers against it.
When ``session.close()`` fires, the watcher drops and the loop
returns.

Step 5 — reuse the session for pipelined requests
-------------------------------------------------

One of HTTP/2's headline wins over 1.1: the session *multiplexes*.
Call
:cpp:func:`request <polycpp::http2::ClientHttp2Session::request>`
again before the previous stream ends and both streams share the same
connection, subject to the server's
``SETTINGS_MAX_CONCURRENT_STREAMS``:

.. code-block:: cpp

   auto s1 = session.request(hdr1);
   auto s2 = session.request(hdr2);   // opens on the same socket as s1

What you learned
----------------

- Set only the :cpp:struct:`Settings <polycpp::http2::Settings>` fields
  you care about; the rest default to the HTTP/2 spec's values.
- Data arrives on ``"data"``; subscribe to ``"response"`` (headers)
  and ``"end"`` (stream closed) to frame the body.
- ``session.close()`` sends GOAWAY and winds down the event loop for
  you.
