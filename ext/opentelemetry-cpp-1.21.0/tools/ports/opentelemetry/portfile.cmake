include(vcpkg_common_functions)

message("CURRENT_PACKAGES_DIR     = ${CURRENT_PACKAGES_DIR}")

message("CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
message("CMAKE_MODULE_PATH        = ${CMAKE_MODULE_PATH}")
message("CMAKE_CURRENT_LIST_DIR   = ${CMAKE_CURRENT_LIST_DIR}")

message("VCPKG_TARGET_TRIPLET     = ${VCPKG_TARGET_TRIPLET}")
message("VCPKG_CMAKE_SYSTEM_NAME  = ${VCPKG_CMAKE_SYSTEM_NAME}")
message("VCPKG_LIBRARY_LINKAGE    = ${VCPKG_LIBRARY_LINKAGE}")

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    stdlib WITH_STDLIB
)

# TODO: if building dynamic, use portable ABI. if building static, use STDLIB
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" BUILD_SHARED)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static"  BUILD_STATIC)

if (NOT DEFINED WIN32)
  execute_process(COMMAND "${CMAKE_CURRENT_LIST_DIR}/get_repo_name.sh" OUTPUT_VARIABLE REPO_NAME ERROR_QUIET)
  message("REPO_NAME=${REPO_NAME}")
endif()

if (DEFINED REPO_NAME)
  # Use local snapshot since we already cloned the code
  get_filename_component(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)
  message("Using local source snapshot from ${SOURCE_PATH}")
else()
  # Fetch from GitHub master
  message("Fetching source code from GitHub...")
  vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO open-telemetry/opentelemetry-cpp
    HEAD_REF master
  )
endif()

# TODO: it will be slightly cleaner to perform pure CMake or Ninja build, by describing all possible variable options
# as separate triplets. Since we have a fairly non-trivial build logic in build.sh script - we use it as-is for now.
# build.sh itself should check if we are building under vcpkg and avoid installing deps that are coming from vcpkg.
if (UNIX)
  set(ENV{USE_VCPKG} 1)
  set(ENV{NOROOT} 1)

  # Custom options could be passed to COMMAND, e.g.
  # vcpkg_execute_build_process(
  #   COMMAND ${SOURCE_PATH}/tools/build.sh nostd-vcpkg -DBUILD_TESTING=OFF
  #   ...
  # 
  vcpkg_execute_build_process(
    COMMAND ${SOURCE_PATH}/tools/build.sh nostd-vcpkg ${FEATURE_OPTIONS}
    WORKING_DIRECTORY ${SOURCE_PATH}/
    LOGNAME build
  )

  vcpkg_execute_build_process(
    COMMAND ${SOURCE_PATH}/tools/install.sh ${CURRENT_PACKAGES_DIR}
    WORKING_DIRECTORY ${SOURCE_PATH}/
    LOGNAME install
  )

else()
  # TODO: verify Windows build
  vcpkg_execute_build_process(
    COMMAND ${SOURCE_PATH}/tools/build.cmd
    WORKING_DIRECTORY ${SOURCE_PATH}/
    LOGNAME build
  # TODO: add Windows headers installation step
 )
endif()

file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
