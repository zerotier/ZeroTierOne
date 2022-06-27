#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "redis++::redis++_static" for configuration "Release"
set_property(TARGET redis++::redis++_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(redis++::redis++_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libredis++.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS redis++::redis++_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_redis++::redis++_static "${_IMPORT_PREFIX}/lib/libredis++.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
