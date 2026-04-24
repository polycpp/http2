Server push alongside a response
================================

Demonstrates :cpp:func:`polycpp::http2::ServerHttp2Stream::pushStream`
— when the browser requests ``/index.html``, the server proactively
pushes ``/app.css`` on the same session so the first render has the
stylesheet without a follow-up round-trip.

.. literalinclude:: ../../../examples/push_asset.cpp
   :language: cpp
   :linenos:

Build and run:

.. code-block:: bash

   cmake -B build -G Ninja -DPOLYCPP_HTTP2_BUILD_EXAMPLES=ON
   cmake --build build --target push_asset
   ./build/examples/push_asset

Note: browsers have largely disabled h2 push in practice
(Chrome dropped it in 2022). The example still runs — pushed streams
are accepted by ``curl --http2-prior-knowledge`` — but production use
usually favours ``103 Early Hints`` or preload link headers instead.
