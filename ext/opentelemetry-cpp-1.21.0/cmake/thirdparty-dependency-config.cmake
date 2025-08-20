# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

#-----------------------------------------------------------------------
# Third party dependencies supported by opentelemetry-cpp
# Dependencies that must be found with find_dependency() when a user calls find_package(opentelemetry-cpp ...)
# should be included in this list.
#-----------------------------------------------------------------------
set(OTEL_THIRDPARTY_DEPENDENCIES_SUPPORTED
     Threads
     ZLIB
     CURL
     nlohmann_json
     Protobuf
     gRPC
     prometheus-cpp
     OpenTracing
)

#-----------------------------------------------------------------------
# Third party dependency target namespaces. Defaults to the dependency's project name if not set.
# Only set if the target namespace is different from the project name (these are case sensitive).
# set(OTEL_<dependency>_TARGET_NAMESPACE "<namespace>")
#-----------------------------------------------------------------------
set(OTEL_Protobuf_TARGET_NAMESPACE "protobuf")

#-----------------------------------------------------------------------
# Set the find_dependecy search mode - empty is default. Options: cmake default (empty string ""), "MODULE", or "CONFIG"
# # set(OTEL_<dependency>_SEARCH_MODE "<search mode>")
#-----------------------------------------------------------------------
set(OTEL_Threads_SEARCH_MODE "")
set(OTEL_ZLIB_SEARCH_MODE "")
set(OTEL_CURL_SEARCH_MODE "")
set(OTEL_nlohmann_json_SEARCH_MODE "CONFIG")
set(OTEL_gRPC_SEARCH_MODE "CONFIG")
set(OTEL_prometheus-cpp_SEARCH_MODE "CONFIG")
set(OTEL_OpenTracing_SEARCH_MODE "CONFIG")

# The search mode is set to "CONFIG" for Protobuf versions >= 3.22.0
# to find Protobuf's abseil dependency properly until the FindProtobuf module is updated support the upstream protobuf-config.cmake.
# See https://gitlab.kitware.com/cmake/cmake/-/issues/24321
if(DEFINED Protobuf_VERSION AND Protobuf_VERSION VERSION_GREATER_EQUAL 3.22.0)
    set(OTEL_Protobuf_SEARCH_MODE "CONFIG")
else()
    set(OTEL_Protobuf_SEARCH_MODE "")
endif()