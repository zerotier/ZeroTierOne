# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(load_plugin_example main.cc)
target_link_libraries(load_plugin_example PRIVATE opentelemetry-cpp::api
                                                  ${CMAKE_DL_LIBS})
