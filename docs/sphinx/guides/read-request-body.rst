How do I read a request body?
=============================

Subscribe to ``"data"`` and ``"end"`` on the
:cpp:class:`ServerHttp2Stream <polycpp::http2::ServerHttp2Stream>` you
received in the handler. HTTP/2 splits the body into DATA frames; each
frame surfaces as one ``"data"`` event.

.. code-block:: cpp

   auto onStream = [](polycpp::http2::ServerHttp2Stream stream, auto hdrs) {
       if (hdrs[":method"] != "POST") {
           stream.respond(405, {});
           stream.end();
           return;
       }

       auto body = std::make_shared<std::string>();
       stream.on("data", [body](const polycpp::buffer::Buffer& chunk) {
           body->append(chunk.toString());
       });
       stream.on("end", [stream, body]() mutable {
           stream.respond(200, {{"content-type", "text/plain"}});
           stream.end("received " + std::to_string(body->size()) + " bytes\n");
       });
   };

For structured bodies (JSON), accumulate into a buffer then parse on
``"end"``; the handler fires on the event-loop thread, so the parse is
already serialised with respect to other handlers.

For very large uploads, accumulating into memory defeats the purpose of
HTTP/2's flow control. Write each chunk straight to disk instead —
backpressure from the file descriptor feeds back through the session's
flow-control window.
