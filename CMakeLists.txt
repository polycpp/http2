cmake_minimum_required(VERSION 3.20)
project(polycpp_http2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

option(POLYCPP_HTTP2_BUILD_TESTS "Build tests" ON)
set(POLYCPP_SOURCE_DIR "" CACHE PATH "Local polycpp source directory")

set(POLYCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(POLYCPP_SOURCE_DIR AND EXISTS "${POLYCPP_SOURCE_DIR}/CMakeLists.txt")
    FetchContent_Declare(polycpp SOURCE_DIR "${POLYCPP_SOURCE_DIR}")
else()
    FetchContent_Declare(
        polycpp
        GIT_REPOSITORY git@github.com:enricohuang/polycpp.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
endif()
FetchContent_MakeAvailable(polycpp)

include(cmake/HTTP2.cmake)

if(POLYCPP_HAS_HTTP2)
    add_library(polycpp_http2 src/http2.cpp)
    target_link_libraries(polycpp_http2 PUBLIC polycpp ${POLYCPP_NGHTTP2_LIB})
    target_include_directories(polycpp_http2 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    foreach(_polycpp_http2_inc IN LISTS POLYCPP_NGHTTP2_INCLUDE_DIR)
        target_include_directories(polycpp_http2 PUBLIC
            $<BUILD_INTERFACE:${_polycpp_http2_inc}>)
    endforeach()
    target_compile_features(polycpp_http2 PUBLIC cxx_std_20)
    target_compile_definitions(polycpp_http2 PUBLIC POLYCPP_HAS_HTTP2=1)
else()
    add_library(polycpp_http2 INTERFACE)
    target_link_libraries(polycpp_http2 INTERFACE polycpp)
    target_include_directories(polycpp_http2 INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endif()

add_library(polycpp::http2 ALIAS polycpp_http2)

if(POLYCPP_HTTP2_BUILD_TESTS AND POLYCPP_HAS_HTTP2)
    enable_testing()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)

    add_executable(test_http2 tests/test_http2.cpp)
    target_link_libraries(test_http2 PRIVATE polycpp_http2 GTest::gtest_main)
    if(MSVC)
        target_compile_options(test_http2 PRIVATE /bigobj)
    endif()
    gtest_discover_tests(test_http2)
endif()
