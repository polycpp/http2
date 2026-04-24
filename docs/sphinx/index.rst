http2
=====

**HTTP/2 client and server**

Build HTTP/2 servers and clients with the same shape as Node's http2 module — Http2Server / Http2SecureServer on one side, connect() / ClientHttp2Session on the other, Http2Stream in the middle. Includes server push, settings negotiation, flow-control hooks, and the event-driven ServerRequest / ServerResponse layer for drop-in request handling.

.. code-block:: cpp

   #include <polycpp/http2/http2.hpp>
   using namespace polycpp;

   EventContext ctx;
   auto server = http2::createServer(ctx, {},
       [](http2::ServerHttp2Stream stream, polycpp::http::Headers /*hdrs*/) {
           stream.respond(200, {{"content-type", "text/plain"}});
           stream.end("hello, http/2\n");
       });
   server.listen(8080);
   ctx.run();

.. grid:: 2

   .. grid-item-card:: Drop-in familiarity
      :margin: 1

      Mirrors the Node.js http2 module API (createServer, createSecureServer, connect, Http2Session, Http2Stream, Http2ServerRequest, Http2ServerResponse) one-to-one.

   .. grid-item-card:: C++20 native
      :margin: 1

      Header-only where possible, zero-overhead abstractions, ``constexpr``
      and ``std::string_view`` throughout.

   .. grid-item-card:: Tested
      :margin: 1

      Ported from the Node.js http2 test corpus plus a Google Test suite that exercises server push, settings, flow-control, and round-trip request handling over nghttp2.

   .. grid-item-card:: Plays well with polycpp
      :margin: 1

      Uses the same JSON value, error, and typed-event types as the rest of
      the polycpp ecosystem — no impedance mismatch.

Getting started
---------------

.. code-block:: bash

   # With FetchContent (recommended)
   FetchContent_Declare(
       polycpp_http2
       GIT_REPOSITORY https://github.com/polycpp/http2.git
       GIT_TAG        master
   )
   FetchContent_MakeAvailable(polycpp_http2)
   target_link_libraries(my_app PRIVATE polycpp::http2)

:doc:`Installation <getting-started/installation>` · :doc:`Quickstart <getting-started/quickstart>` · :doc:`Tutorials <tutorials/index>` · :doc:`API reference <api/index>`

.. toctree::
   :hidden:
   :caption: Getting started

   getting-started/installation
   getting-started/quickstart

.. toctree::
   :hidden:
   :caption: Tutorials

   tutorials/index

.. toctree::
   :hidden:
   :caption: How-to guides

   guides/index

.. toctree::
   :hidden:
   :caption: API reference

   api/index

.. toctree::
   :hidden:
   :caption: Examples

   examples/index
