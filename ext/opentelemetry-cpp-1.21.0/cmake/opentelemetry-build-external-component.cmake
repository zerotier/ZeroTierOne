# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

function(get_directory_name_in_path PATH_VAR RESULT_VAR)
  # get_filename_component does not work with paths ending in / or \, so remove it.
  string(REGEX REPLACE "[/\\]$" "" PATH_TRIMMED "${PATH_VAR}")

  get_filename_component(DIR_NAME ${PATH_TRIMMED} NAME)

  set(${RESULT_VAR} "${DIR_NAME}" PARENT_SCOPE)
endfunction()

# Enable building external components through otel-cpp build
# The config options are
#  - OPENTELEMETRY_EXTERNAL_COMPONENT_PATH - Setting local paths of the external
# component as env variable. Multiple paths can be set by separating them with.
#  - OPENTELEMETRY_EXTERNAL_COMPONENT_URL Setting github-repo of external component
#  as env variable

# Add custom vendor component from local path, or GitHub repo
# Prefer CMake option, then env variable, then URL.
if(OPENTELEMETRY_EXTERNAL_COMPONENT_PATH)
  # Add custom component path to build tree and consolidate binary artifacts in
  # current project binary output directory.
  foreach(DIR IN LISTS OPENTELEMETRY_EXTERNAL_COMPONENT_PATH)
    get_directory_name_in_path(${DIR} EXTERNAL_EXPORTER_DIR_NAME)
    add_subdirectory(${DIR} ${PROJECT_BINARY_DIR}/external/${EXTERNAL_EXPORTER_DIR_NAME})
  endforeach()
elseif(DEFINED ENV{OPENTELEMETRY_EXTERNAL_COMPONENT_PATH})
  # Add custom component path to build tree and consolidate binary artifacts in
  # current project binary output directory.
  set(OPENTELEMETRY_EXTERNAL_COMPONENT_PATH_VAR $ENV{OPENTELEMETRY_EXTERNAL_COMPONENT_PATH})
  foreach(DIR IN LISTS OPENTELEMETRY_EXTERNAL_COMPONENT_PATH_VAR)
    get_directory_name_in_path(${DIR} EXTERNAL_EXPORTER_DIR_NAME)
    add_subdirectory(${DIR} ${PROJECT_BINARY_DIR}/external/${EXTERNAL_EXPORTER_DIR_NAME})
  endforeach()
elseif(DEFINED $ENV{OPENTELEMETRY_EXTERNAL_COMPONENT_URL})
  # This option requires CMake 3.11+: add standard remote repo to build tree.
  include(FetchContent)
  FetchContent_Declare(
    external
    GIT_REPOSITORY $ENV{OPENTELEMETRY_EXTERNAL_COMPONENT_URL}
    GIT_TAG "main")
  FetchContent_GetProperties(external)
  if(NOT external_POPULATED)
    FetchContent_Populate(external)
    add_subdirectory(${external_SOURCE_DIR})
  endif()
endif()
