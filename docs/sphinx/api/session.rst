Session
=======

An :cpp:class:`Http2Session <polycpp::http2::Http2Session>` represents
one HTTP/2 connection. Server-accepted sessions are
:cpp:class:`ServerHttp2Session <polycpp::http2::ServerHttp2Session>`;
client-initiated ones are
:cpp:class:`ClientHttp2Session <polycpp::http2::ClientHttp2Session>`.
Both are thin value-type wrappers over a shared impl — copying them is
cheap and all copies refer to the same session.

Http2Session (base)
-------------------

.. doxygenclass:: polycpp::http2::Http2Session
   :members:
   :undoc-members:

ServerHttp2Session
------------------

.. doxygenclass:: polycpp::http2::ServerHttp2Session
   :members:
   :undoc-members:

ClientHttp2Session
------------------

.. doxygenclass:: polycpp::http2::ClientHttp2Session
   :members:
   :undoc-members:

Options and state
-----------------

.. doxygenstruct:: polycpp::http2::SessionOptions
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::SessionState
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::AlternativeServiceOptions
   :members:
   :undoc-members:
