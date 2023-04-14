project(prometheus-cpp-lite)
cmake_minimum_required(VERSION 3.2)

option(PROMETHEUS_BUILD_EXAMPLES "Build with examples" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

if(WIN32) 

  # it prevent create Debug/ and Release folders in Visual Studio
  foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_SOURCE_DIR}/bin )
  endforeach()

  set (INSTALL_PATH_BIN "${PROJECT_SOURCE_DIR}/installed/bin/")

else() # not WIN32

  set (INSTALL_PATH_BIN "bin/")

endif()

add_subdirectory("./core")

add_subdirectory("./simpleapi")

add_subdirectory("./3rdpatry/http-client-lite")

if(PROMETHEUS_BUILD_EXAMPLES)
  add_subdirectory("./examples")
endif()


