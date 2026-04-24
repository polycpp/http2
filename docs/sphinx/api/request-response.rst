Server request and response
===========================

The Node-style "express-like" handler layer sits on top of
:cpp:class:`ServerHttp2Stream <polycpp::http2::ServerHttp2Stream>`. Use
it when you want a familiar incoming-message / outgoing-message pair;
skip it and work with the bare stream if you're doing bulk transfer or
server push.

Http2ServerRequest
------------------

.. doxygenclass:: polycpp::http2::Http2ServerRequest
   :members:
   :undoc-members:

Http2ServerResponse
-------------------

.. doxygenclass:: polycpp::http2::Http2ServerResponse
   :members:
   :undoc-members:
