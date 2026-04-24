How do I inspect flow-control windows and connection stats?
===========================================================

Call :cpp:func:`Http2Session::state <polycpp::http2::Http2Session::state>`
and :cpp:func:`Http2Stream::state <polycpp::http2::Http2Stream::state>`
— they return :cpp:struct:`SessionState <polycpp::http2::SessionState>`
and :cpp:struct:`StreamState <polycpp::http2::StreamState>` snapshots
respectively.

.. code-block:: cpp

   auto s = session.state();
   std::cerr << "  session window local: "  << s.localWindowSize
             << "  remote: "                << s.remoteWindowSize
             << "  streamCount: "           << s.streamCount
             << '\n';

   for (auto& stream : stream_pool) {
       auto st = stream.state();
       std::cerr << "  stream window: "     << st.localWindowSize
                 << " state: "              << st.state
                 << '\n';
   }

Poll from a periodic timer when diagnosing stalls — flow-control is
the usual culprit when HTTP/2 throughput mysteriously drops under load.

For per-session settings snapshots, check
:cpp:func:`Http2Session::localSettings <polycpp::http2::Http2Session::localSettings>`
and
:cpp:func:`Http2Session::remoteSettings <polycpp::http2::Http2Session::remoteSettings>` —
both return a :cpp:struct:`Settings <polycpp::http2::Settings>` with
every field populated with the currently negotiated value.
