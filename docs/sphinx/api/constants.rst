Constants
=========

``polycpp::http2::constants`` mirrors Node's ``http2.constants``
namespace. It holds every HTTP/2 error code, frame type, settings ID,
standard status code, pseudo-header name, method name, and common
header name — all as ``constexpr`` values, safe to use in
``static_assert`` and in switch statements over protocol state.

.. doxygennamespace:: polycpp::http2::constants
