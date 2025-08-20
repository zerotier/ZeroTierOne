# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(http)
if(BUILD_W3CTRACECONTEXT_TEST)
  add_subdirectory(w3c_tracecontext_http_test_server)
endif()
