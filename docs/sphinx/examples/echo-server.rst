HTTP/2 echo server
==================

A minimal server that echoes each request body back as the response.
Useful as a smoke test — ``curl --http2-prior-knowledge`` against it and
you should see your payload come back byte-for-byte.

.. literalinclude:: ../../../examples/echo_server.cpp
   :language: cpp
   :linenos:

Build and run:

.. code-block:: bash

   cmake -B build -G Ninja -DPOLYCPP_HTTP2_BUILD_EXAMPLES=ON
   cmake --build build --target echo_server
   ./build/examples/echo_server &
   curl --http2-prior-knowledge -X POST -d 'hello' http://localhost:8080/
