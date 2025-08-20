# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(common_metrics_foo_library foo_library.h foo_library.cc)

set_target_version(common_metrics_foo_library)

if(DEFINED OPENTELEMETRY_BUILD_DLL)
  target_compile_definitions(common_metrics_foo_library
                             PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
endif()

target_include_directories(common_metrics_foo_library
                           PUBLIC $<BUILD_INTERFACE:${EXAMPLES_COMMON_DIR}>)

target_link_libraries(common_metrics_foo_library PUBLIC opentelemetry-cpp::api)
