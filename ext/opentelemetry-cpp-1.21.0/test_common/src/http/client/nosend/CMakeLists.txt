# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(${BUILD_TESTING})
  add_library(opentelemetry_http_client_nosend http_client_test_factory.cc
                                               http_client_nosend.cc)

  set_target_properties(opentelemetry_http_client_nosend
                        PROPERTIES EXPORT_NAME opentelemetry_http_client_nosend)

  target_link_libraries(
    opentelemetry_http_client_nosend opentelemetry_ext
    opentelemetry_test_common ${GMOCK_LIB} ${GTEST_BOTH_LIBRARIES})

endif()
