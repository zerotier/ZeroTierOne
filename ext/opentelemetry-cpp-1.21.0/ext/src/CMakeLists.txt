# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(WITH_HTTP_CLIENT_CURL)
  add_subdirectory(http/client/curl)
endif()

if(DEFINED OPENTELEMETRY_BUILD_DLL)
  add_subdirectory(dll)
endif()
