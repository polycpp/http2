Quickstart
==========

This page walks through a minimal http2 program end-to-end. Copy the
snippet, run it, then jump to :doc:`../tutorials/index` for task-oriented
walkthroughs or :doc:`../api/index` for the full reference.

We'll stand up a plain-text (``h2c``) HTTP/2 server on a local port,
handle one request per stream, and respond with a small JSON body. This
is the shortest complete program that exercises a server, a session, a
stream, and the response-helper chain in one place.

Full example
------------

.. code-block:: cpp

   #include <iostream>
   #include <polycpp/http2/http2.hpp>
   #include <polycpp/io/event_context.hpp>

   using namespace polycpp;

   int main() {
       EventContext ctx;

       http2::ServerOptions opts;  // defaults are fine for h2c
       auto server = http2::createServer(ctx, opts,
           [](http2::ServerHttp2Stream stream, polycpp::http::Headers hdrs) {
               // hdrs[":path"], hdrs[":method"], ...
               stream.respond(200, {
                   {"content-type", "application/json"},
                   {"server",       "polycpp-http2/0.1"},
               });
               stream.end(R"({"status":"ok"})");
           });

       // listen(0) asks the OS for a free port; use a fixed port in production.
       server.listen(8080);
       std::cout << "listening on http://localhost:8080\n";

       ctx.run();   // drives the event loop
   }

Compile it with the same CMake wiring from :doc:`installation`:

.. code-block:: bash

   cmake -B build -G Ninja
   cmake --build build
   ./build/my_app

Test with curl (the ``--http2-prior-knowledge`` flag skips the
Upgrade dance and speaks h2c directly):

.. code-block:: bash

   curl --http2-prior-knowledge http://localhost:8080/ -i

Expected output:

.. code-block:: text

   HTTP/2 200
   content-type: application/json
   server: polycpp-http2/0.1

   {"status":"ok"}

What just happened
------------------

1. :cpp:func:`http2::createServer <polycpp::http2::createServer>`
   returned an :cpp:class:`Http2Server <polycpp::http2::Http2Server>`.
   The ``onStream`` callback fires once per inbound HTTP/2 stream — a
   stream here is the HTTP/2 analogue of a request/response pair.

2. The handler received a
   :cpp:class:`ServerHttp2Stream <polycpp::http2::ServerHttp2Stream>`
   and a ``polycpp::http::Headers`` map of the client's request
   headers (including the ``:path``, ``:method``, ``:scheme``, and
   ``:authority`` pseudo-headers HTTP/2 mandates).

3. ``stream.respond(200, {...})`` wrote the response pseudo-header and
   user headers in a single HEADERS frame; ``stream.end(body)`` closed
   the stream with a final DATA frame. Use ``stream.write(chunk)`` in
   between for multi-chunk bodies — see :doc:`../tutorials/push-an-asset`.

4. ``ctx.run()`` drives the event loop. The server, sessions, and
   streams are all driven off its file-descriptor watchers — nothing
   happens until you ``run``.

For a TLS (``h2``) server, swap
:cpp:func:`createServer <polycpp::http2::createServer>` for
:cpp:func:`createSecureServer <polycpp::http2::createSecureServer>` and
fill in
:cpp:class:`SecureServerOptions <polycpp::http2::SecureServerOptions>`'s
``cert``, ``key``, and ``ca`` fields.

Next steps
----------

- :doc:`../tutorials/index` — step-by-step walkthroughs of common tasks.
- :doc:`../guides/index` — short how-tos for specific problems.
- :doc:`../api/index` — every public type, function, and option.
- :doc:`../examples/index` — runnable programs you can drop into a sandbox.
