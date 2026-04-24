Settings
========

HTTP/2 peers exchange a ``SETTINGS`` frame early in every connection
that negotiates flow-control window sizes, header-table caps, push
support, and more.
:cpp:struct:`polycpp::http2::Settings` holds the
local view; :cpp:func:`Http2Session::settings <polycpp::http2::Http2Session::settings>`
sends a new one.

Settings struct
---------------

.. doxygenstruct:: polycpp::http2::Settings
   :members:
   :undoc-members:

Helpers
-------

.. doxygenfunction:: polycpp::http2::getDefaultSettings
.. doxygenfunction:: polycpp::http2::getPackedSettings
.. doxygenfunction:: polycpp::http2::getUnpackedSettings
.. doxygenfunction:: polycpp::http2::validateSettings
