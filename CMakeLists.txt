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
        GIT_TAG        feature/1239-companion-split
        GIT_SHALLOW    TRUE
    )
endif()
FetchContent_MakeAvailable(polycpp)

function(polycpp_apply_base_config_contract target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Cannot apply the polycpp configuration contract to missing target ${target_name}.")
    endif()
    if(NOT TARGET polycpp)
        message(FATAL_ERROR "${target_name} requires the base polycpp target before applying the configuration contract.")
    endif()

    get_target_property(_polycpp_base_compatible_properties
        polycpp COMPATIBLE_INTERFACE_STRING)
    if(NOT _polycpp_base_compatible_properties OR _polycpp_base_compatible_properties STREQUAL "_polycpp_base_compatible_properties-NOTFOUND")
        message(FATAL_ERROR
            "${target_name} requires a base polycpp target that exposes a POLYCPP_CONFIG_* "
            "compatible-interface contract. Update the base polycpp CMake package.")
    endif()

    set(_polycpp_contract_properties)
    foreach(_polycpp_contract_property IN LISTS _polycpp_base_compatible_properties)
        if(_polycpp_contract_property MATCHES "^POLYCPP_CONFIG_")
            list(APPEND _polycpp_contract_properties "${_polycpp_contract_property}")
        endif()
    endforeach()
    if(NOT _polycpp_contract_properties)
        message(FATAL_ERROR
            "${target_name} requires a base polycpp target with POLYCPP_CONFIG_* "
            "compatible-interface properties. Update the base polycpp CMake package.")
    endif()

    set_property(TARGET ${target_name} APPEND PROPERTY
        COMPATIBLE_INTERFACE_STRING ${_polycpp_contract_properties})

    foreach(_polycpp_contract_property IN LISTS _polycpp_contract_properties)
        get_target_property(_polycpp_contract_value
            polycpp INTERFACE_${_polycpp_contract_property})
        if(NOT DEFINED _polycpp_contract_value OR _polycpp_contract_value STREQUAL "_polycpp_contract_value-NOTFOUND")
            message(FATAL_ERROR
                "${target_name} requires a base polycpp target that exposes "
                "INTERFACE_${_polycpp_contract_property}. Update the base polycpp CMake package.")
        endif()
        set_property(TARGET ${target_name} PROPERTY
            INTERFACE_${_polycpp_contract_property} "${_polycpp_contract_value}")
    endforeach()
endfunction()


include(cmake/HTTP2.cmake)

if(NOT POLYCPP_HAS_HTTP2)
    message(FATAL_ERROR "polycpp_http2 requires nghttp2 to configure successfully.")
endif()

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

add_library(polycpp::http2 ALIAS polycpp_http2)
polycpp_apply_base_config_contract(polycpp_http2)


if(POLYCPP_HTTP2_BUILD_TESTS)
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
