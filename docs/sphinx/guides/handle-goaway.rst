How do I shut a session down cleanly?
=====================================

Call :cpp:func:`Http2Session::close <polycpp::http2::Http2Session::close>`.
Under the hood this sends a ``GOAWAY`` frame with the last stream ID the
peer can still expect, drains in-flight streams, and closes the socket.

.. code-block:: cpp

   session.close();          // graceful

   // Optional: subscribe before the call if you need a completion hook.
   session.once("close", []() { /* socket closed */ });

For a hard shutdown (CTRL-C on a misbehaving peer), use
:cpp:func:`destroy <polycpp::http2::Http2Session::destroy>` instead —
it tears the socket down without draining:

.. code-block:: cpp

   polycpp::Error e("shutting down");
   session.destroy(e);

On the server side, register a signal handler against your
:cpp:class:`EventContext` and call ``server.close()`` — it stops
accepting new connections, drains active sessions via GOAWAY, and then
returns.
