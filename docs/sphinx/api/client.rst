Client
======

Client-side connections are built from
:cpp:func:`connect <polycpp::http2::connect>`, which hands you a
:cpp:class:`ClientHttp2Session <polycpp::http2::ClientHttp2Session>` —
call :cpp:func:`request <polycpp::http2::ClientHttp2Session::request>`
on it to open a
:cpp:class:`ClientHttp2Stream <polycpp::http2::ClientHttp2Stream>`.

Factories
---------

.. doxygenfunction:: polycpp::http2::connect(EventContext&, const std::string&, const ClientSessionOptions&, std::function<void(ClientHttp2Session&)>)
.. doxygenfunction:: polycpp::http2::connect(EventContext&, const std::string&, const SecureClientSessionOptions&, std::function<void(ClientHttp2Session&)>)

Options
-------

.. doxygenstruct:: polycpp::http2::ClientSessionOptions
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::SecureClientSessionOptions
   :members:
   :undoc-members:

.. doxygenstruct:: polycpp::http2::ClientRequestOptions
   :members:
   :undoc-members:
