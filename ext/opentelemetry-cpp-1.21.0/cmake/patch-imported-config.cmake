# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Some prebuilt or installed targets may have different CONFIG settings than
# what we use to configure otel-cpp. This file applies patches to the imported
# targets in order to use compatible CONFIG settings for fallback.

# Common dependencies
project_build_tools_patch_default_imported_config(ZLIB::ZLIB)

# protobuf targets
if(Protobuf_FOUND)
  project_build_tools_patch_default_imported_config(
    utf8_range::utf8_range utf8_range::utf8_validity protobuf::libprotobuf-lite
    protobuf::libprotobuf protobuf::libprotoc)
endif()

# cares targets
if(TARGET c-ares::cares)
  project_build_tools_patch_default_imported_config(c-ares::cares)
endif()

# curl targets
if(TARGET CURL::libcurl
   OR TARGET CURL::libcurl_static
   OR TARGET CURL::libcurl_shared)
  project_build_tools_patch_default_imported_config(
    CURL::libcurl CURL::libcurl_static CURL::libcurl_shared)
endif()

# abseil targets
if(TARGET absl::bad_variant_access)
  project_build_tools_patch_default_imported_config(
    absl::bad_variant_access
    absl::raw_logging_internal
    absl::log_severity
    absl::log_internal_check_op
    absl::log_internal_nullguard
    absl::strings
    absl::strings_internal
    absl::base
    absl::spinlock_wait
    absl::int128
    absl::throw_delegate
    absl::log_internal_message
    absl::examine_stack
    absl::stacktrace
    absl::debugging_internal
    absl::symbolize
    absl::demangle_internal
    absl::malloc_internal
    absl::log_internal_format
    absl::log_internal_globals
    absl::time
    absl::civil_time
    absl::time_zone
    absl::str_format_internal
    absl::log_internal_proto
    absl::log_internal_log_sink_set
    absl::log_globals
    absl::hash
    absl::city
    absl::bad_optional_access
    absl::low_level_hash
    absl::log_entry
    absl::log_sink
    absl::synchronization
    absl::graphcycles_internal
    absl::strerror
    absl::log_internal_conditions
    absl::cord
    absl::cord_internal
    absl::crc_cord_state
    absl::crc32c
    absl::crc_cpu_detect
    absl::crc_internal
    absl::cordz_functions
    absl::exponential_biased
    absl::cordz_info
    absl::cordz_handle
    absl::leak_check
    absl::die_if_null
    absl::flags
    absl::flags_commandlineflag
    absl::flags_commandlineflag_internal
    absl::flags_config
    absl::flags_program_name
    absl::flags_internal
    absl::flags_marshalling
    absl::flags_reflection
    absl::flags_private_handle_accessor
    absl::raw_hash_set
    absl::hashtablez_sampler
    absl::log_initialize
    absl::status
    absl::statusor)
endif()

# gRPC targets
if(TARGET gRPC::grpc++)
  project_build_tools_patch_default_imported_config(
    gRPC::cares
    gRPC::re2
    gRPC::ssl
    gRPC::crypto
    gRPC::zlibstatic
    gRPC::address_sorting
    gRPC::gpr
    gRPC::grpc
    gRPC::grpc_unsecure
    gRPC::grpc++
    gRPC::grpc++_alts
    gRPC::grpc++_error_details
    gRPC::grpc++_reflection
    gRPC::grpc++_unsecure
    gRPC::grpc_authorization_provider
    gRPC::grpc_plugin_support
    gRPC::grpcpp_channelz
    gRPC::upb)
endif()

# prometheus targets
if(TARGET prometheus-cpp::core)
  project_build_tools_patch_default_imported_config(
    prometheus-cpp::core prometheus-cpp::pull prometheus-cpp::push)
endif()

# civetweb targets
if(TARGET civetweb::civetweb)
  project_build_tools_patch_default_imported_config(
    civetweb::civetweb civetweb::server civetweb::civetweb-cpp)
endif()

if(BUILD_TESTING)
  project_build_tools_patch_default_imported_config(
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
    GTest::GTest
    GTest::Main
    benchmark::benchmark
    benchmark::benchmark_main)
endif()
