# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

#
# The dependency on nlohmann_json can be provided different ways. By order of
# decreasing priority, options are:
#
# 1 - Search for a nlohmann_json package
#
# Packages installed on the local machine are used if found.
#
# The nlohmann_json dependency is not installed, as it already is.
#
# 2 - Search for a nlohmann_json git submodule
#
# When git submodule is used, the nlohmann_json code is located in:
# third_party/nlohmann-json
#
# The nlohmann_json dependency is installed, by building the sub directory with
# JSON_Install=ON
#
# 3 - Download nlohmann_json from github
#
# Code from the development branch is used, unless a specific release tag is
# provided in variable ${nlohmann-json}
#
# The nlohmann_json dependency is installed, by building the downloaded code
# with JSON_Install=ON
#

# nlohmann_json package is required for most SDK build configurations
find_package(nlohmann_json QUIET)
set(nlohmann_json_clone FALSE)
if(nlohmann_json_FOUND)
  message(STATUS "nlohmann::json dependency satisfied by: package")
elseif(TARGET nlohmann_json)
  message(STATUS "nlohmann::json is already added as a CMake target!")
elseif(EXISTS ${PROJECT_SOURCE_DIR}/.git
       AND EXISTS
           ${PROJECT_SOURCE_DIR}/third_party/nlohmann-json/CMakeLists.txt)
  message(STATUS "nlohmann::json dependency satisfied by: git submodule")
  set(JSON_BuildTests
      OFF
      CACHE INTERNAL "")
  set(JSON_Install
      ON
      CACHE INTERNAL "")
  # This option allows to link nlohmann_json::nlohmann_json target
  add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/nlohmann-json)
  # This option allows to add header to include directories
  include_directories(
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann-json/single_include)
else()
  if("${nlohmann-json}" STREQUAL "")
    set(nlohmann-json "develop")
  endif()
  message(STATUS "nlohmann::json dependency satisfied by: github download")
  set(nlohmann_json_clone TRUE)
  include(ExternalProject)
  ExternalProject_Add(
    nlohmann_json_download
    PREFIX third_party
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG "${nlohmann-json}"
    UPDATE_COMMAND ""
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
               -DJSON_BuildTests=OFF -DJSON_Install=ON
               -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    TEST_AFTER_INSTALL 0
    DOWNLOAD_NO_PROGRESS 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    LOG_INSTALL 1)

  ExternalProject_Get_Property(nlohmann_json_download INSTALL_DIR)
  set(NLOHMANN_JSON_INCLUDE_DIR
      ${INSTALL_DIR}/src/nlohmann_json_download/single_include)
  add_library(nlohmann_json_ INTERFACE)
  target_include_directories(
    nlohmann_json_ INTERFACE "$<BUILD_INTERFACE:${NLOHMANN_JSON_INCLUDE_DIR}>"
                             "$<INSTALL_INTERFACE:include>")
  add_dependencies(nlohmann_json_ nlohmann_json_download)
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json_)

  if(OPENTELEMETRY_INSTALL)
    install(
      TARGETS nlohmann_json_
      EXPORT "${PROJECT_NAME}-third_party_nlohmann_json_target"
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      COMPONENT third_party_nlohmann_json)
  endif()
endif()
