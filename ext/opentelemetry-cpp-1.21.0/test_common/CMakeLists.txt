# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(BUILD_TESTING)
  add_library(opentelemetry_test_common INTERFACE)
  target_include_directories(
    opentelemetry_test_common
    INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
              "$<INSTALL_INTERFACE:include>")

  set_target_properties(opentelemetry_test_common PROPERTIES EXPORT_NAME
                                                             "teset_common")
  target_link_libraries(opentelemetry_test_common INTERFACE opentelemetry_api)

  add_subdirectory(src)
endif()
