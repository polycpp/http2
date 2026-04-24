How do I serve a file without buffering it?
===========================================

Use :cpp:func:`ServerHttp2Stream::respondWithFile <polycpp::http2::ServerHttp2Stream::respondWithFile>`
— it opens the file, derives ``content-length``, streams the data via
``sendfile``-style I/O where available, and closes the stream when the
body is done:

.. code-block:: cpp

   auto onStream = [](polycpp::http2::ServerHttp2Stream stream, auto hdrs) {
       if (hdrs[":path"] == "/report.pdf") {
           stream.respondWithFile("/var/app/report.pdf",
               polycpp::http::Headers{{"content-type", "application/pdf"}});
           return;
       }
       stream.respond(404, {});
       stream.end();
   };

If the file is missing or unreadable, the stream emits ``"error"`` with
a :cpp:class:`polycpp::Error` describing the failure — subscribe before
calling ``respondWithFile`` if you want to fall back to a 404 programmatically.

Pass
:cpp:struct:`StreamFileResponseOptions <polycpp::http2::StreamFileResponseOptions>`
to override offset/length (for range requests) or to skip ``stat``
and trust the length you supply.
