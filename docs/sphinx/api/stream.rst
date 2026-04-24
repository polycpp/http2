Stream
======

A stream is the HTTP/2 analogue of a single request/response pair — one
pair of HEADERS frames plus any DATA frames between them. Every
:cpp:class:`Http2Stream <polycpp::http2::Http2Stream>` lives inside
exactly one session; sessions multiplex many streams concurrently.

Http2Stream (base)
------------------

.. doxygenclass:: polycpp::http2::Http2Stream
   :members:
   :undoc-members:

ServerHttp2Stream
-----------------

Server-side streams receive requests and emit responses. These are the
streams handed to your ``createServer`` callback.

.. doxygenclass:: polycpp::http2::ServerHttp2Stream
   :members:
   :undoc-members:

ClientHttp2Stream
-----------------

Client-side streams are opened by
:cpp:func:`ClientHttp2Session::request <polycpp::http2::ClientHttp2Session::request>`.

.. doxygenclass:: polycpp::http2::ClientHttp2Stream
   :members:
   :undoc-members:

Options and state
-----------------

.. doxygenstruct:: polycpp::http2::StreamResponseOptions
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::StreamFileResponseOptions
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::StreamState
   :members:
   :undoc-members:

Pseudo-headers
--------------

.. doxygenstruct:: polycpp::http2::Http2RequestMeta
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::Http2ResponseMeta
   :members:
   :undoc-members:
