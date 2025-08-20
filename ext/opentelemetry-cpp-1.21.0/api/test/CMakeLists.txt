# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(core)
add_subdirectory(context)
add_subdirectory(plugin)
add_subdirectory(nostd)
add_subdirectory(trace)
add_subdirectory(metrics)
add_subdirectory(logs)
add_subdirectory(common)
add_subdirectory(baggage)

if(NOT OPENTELEMETRY_SKIP_DYNAMIC_LOADING_TESTS)
  add_subdirectory(singleton)
endif()
