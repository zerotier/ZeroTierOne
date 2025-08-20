# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(load)

if(NOT OPENTELEMETRY_SKIP_DYNAMIC_LOADING_TESTS)
  add_subdirectory(plugin)
endif()
