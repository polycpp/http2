Server
======

:cpp:func:`createServer <polycpp::http2::createServer>` and its secure
cousin :cpp:func:`createSecureServer <polycpp::http2::createSecureServer>`
are the two entry points for server-side code. Each returns a value-type
:cpp:class:`Http2Server <polycpp::http2::Http2Server>` (or
:cpp:class:`Http2SecureServer <polycpp::http2::Http2SecureServer>`) that
owns the listening socket and fans out accepted sessions.

Factories
---------

.. doxygenfunction:: polycpp::http2::createServer
.. doxygenfunction:: polycpp::http2::createSecureServer

Http2Server
-----------

.. doxygenclass:: polycpp::http2::Http2Server
   :members:
   :undoc-members:

Http2SecureServer
-----------------

.. doxygenclass:: polycpp::http2::Http2SecureServer
   :members:
   :undoc-members:

Options
-------

.. doxygenstruct:: polycpp::http2::ServerOptions
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::SecureServerOptions
   :members:
   :undoc-members:
