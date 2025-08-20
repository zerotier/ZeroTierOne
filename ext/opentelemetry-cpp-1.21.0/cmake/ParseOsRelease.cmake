# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Parse /etc/os-release to determine Linux distro

if(EXISTS /etc/os-release)
file(STRINGS /etc/os-release OS_RELEASE)
foreach(NameAndValue ${OS_RELEASE})
  # Strip leading spaces
  string(REGEX REPLACE "^[ ]+" "" NameAndValue ${NameAndValue})
  # Find variable name
  string(REGEX MATCH "^[^=]+" Name ${NameAndValue})
  # Find the value
  string(REPLACE "${Name}=" "" Value ${NameAndValue})
  # Strip quotes from value
  string(REPLACE "\"" "" Value ${Value})
  # Set the variable
  message("-- /etc/os-release : ${Name}=${Value}")
  set("OS_RELEASE_${Name}" "${Value}")
endforeach()
else()
  set("OS_RELEASE_NAME" ${CMAKE_SYSTEM_NAME})
  set("OS_RELEASE_ID" ${CMAKE_SYSTEM_NAME})
  set("OS_RELEASE_VERSION_ID" "1.0")
endif()
