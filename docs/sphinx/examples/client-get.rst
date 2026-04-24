HTTP/2 client GET
=================

Single-request client that opens an h2 session, issues a ``GET``, prints
response headers and body, then closes. Exits cleanly with the response
status as its exit code so you can chain it in scripts.

.. literalinclude:: ../../../examples/client_get.cpp
   :language: cpp
   :linenos:

Build and run:

.. code-block:: bash

   cmake -B build -G Ninja -DPOLYCPP_HTTP2_BUILD_EXAMPLES=ON
   cmake --build build --target client_get
   ./build/examples/client_get http://localhost:8080/
