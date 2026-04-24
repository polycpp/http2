How do I run a TLS (``h2``) server?
====================================

Use :cpp:func:`createSecureServer <polycpp::http2::createSecureServer>`
and set ``cert``, ``key``, and optionally ``ca`` on
:cpp:class:`SecureServerOptions <polycpp::http2::SecureServerOptions>`:

.. code-block:: cpp

   #include <polycpp/http2/http2.hpp>
   #include <polycpp/io/event_context.hpp>

   polycpp::EventContext ctx;
   polycpp::http2::SecureServerOptions opts;
   opts.cert = readFile("server.crt");
   opts.key  = readFile("server.key");

   auto server = polycpp::http2::createSecureServer(ctx, opts,
       [](auto stream, auto) {
           stream.respond(200, {{"content-type", "text/plain"}});
           stream.end("secure hello\n");
       });
   server.listen(8443);
   ctx.run();

ALPN is hard-wired to ``h2`` today. For mutual TLS, populate
``opts.ca`` with the CA bundle you'll verify client certs against.

Production checklist:

- Pin a modern cipher suite via the OpenSSL/BoringSSL backend your
  polycpp build uses — the http2 library doesn't filter the server
  context.
- Rotate the certificate on disk and restart the server, or adopt
  your own re-load hook; the current implementation reads cert/key at
  ``createSecureServer`` time.
