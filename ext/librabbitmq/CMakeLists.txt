cmake_minimum_required(VERSION 2.8.12)
project(rabbitmq-c "C")

# Enable MACOSX_RPATH by default. See: cmake --help-policy CMP0042
if (POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Follow all steps below in order to calculate new ABI version when updating the library
# NOTE: THIS IS UNRELATED to the actual project version
#
# 1. If the library source code has changed at all since the last update, then increment revision
# 2. If any interfaces have been added, removed, or changed since the last update, increment current and set revision to 0.
# 3. If any interfaces have been added since the last public release, then increment age.
# 4. If any interfaces have been removed since the last public release, then set age to 0.

set(RMQ_SOVERSION_CURRENT   7)
set(RMQ_SOVERSION_REVISION  0)
set(RMQ_SOVERSION_AGE       3)

math(EXPR RMQ_SOVERSION_MAJOR "${RMQ_SOVERSION_CURRENT} - ${RMQ_SOVERSION_AGE}")
math(EXPR RMQ_SOVERSION_MINOR "${RMQ_SOVERSION_AGE}")
math(EXPR RMQ_SOVERSION_PATCH "${RMQ_SOVERSION_REVISION}")

set(RMQ_VERSION ${RMQ_SOVERSION_MAJOR}.${RMQ_SOVERSION_MINOR}.${RMQ_SOVERSION_PATCH})
set(RMQ_SOVERSION ${RMQ_SOVERSION_MAJOR})

file(STRINGS librabbitmq/amqp.h _API_VERSION_MAJOR REGEX "^#define AMQP_VERSION_MAJOR [0-9]+$")
file(STRINGS librabbitmq/amqp.h _API_VERSION_MINOR REGEX "^#define AMQP_VERSION_MINOR [0-9]+$")
file(STRINGS librabbitmq/amqp.h _API_VERSION_PATCH REGEX "^#define AMQP_VERSION_PATCH [0-9]+$")

string(REGEX MATCH "[0-9]+" _API_VERSION_MAJOR ${_API_VERSION_MAJOR})
string(REGEX MATCH "[0-9]+" _API_VERSION_MINOR ${_API_VERSION_MINOR})
string(REGEX MATCH "[0-9]+" _API_VERSION_PATCH ${_API_VERSION_PATCH})

# VERSION to match what is in autotools
set(VERSION ${_API_VERSION_MAJOR}.${_API_VERSION_MINOR}.${_API_VERSION_PATCH})

if (CMAKE_GENERATOR MATCHES ".*(Make|Ninja).*"
    AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
  message(STATUS "CMAKE_BUILD_TYPE not specified. Creating ${CMAKE_BUILD_TYPE} build")
endif()

include(TestCInline)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(CMakePushCheckState)
include(GNUInstallDirs)
include(CheckCCompilerFlag)

# Detect if we need to link against a socket library:
cmake_push_check_state()
if (WIN32)
  # Always use WinSock2 on Windows
  set(SOCKET_LIBRARIES ws2_32)
else ()
  # Is it in the default link?
  check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO)
  if (NOT (HAVE_GETADDRINFO EQUAL 1))
    SET(CMAKE_REQUIRED_LIBRARIES "socket")
    check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO2)
    if (HAVE_GETADDRINFO2 EQUAL 1)
      set(SOCKET_LIBRARIES socket)
    else ()
      SET(CMAKE_REQUIRED_LIBRARIES "socket;nsl")
      check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO3)
      if (HAVE_GETADDRINFO3 EQUAL 1)
        set(SOCKET_LIBRARIES socket nsl)
      else ()
        message(FATAL_ERROR "Cannot find name resolution library (containing symbol getaddrinfo)")
      endif ()
    endif ()
  endif ()

  set(CMAKE_REQUIRED_LIBRARIES ${SOCKET_LIBRARIES})
  check_symbol_exists(socket "sys/types.h;sys/socket.h" HAVE_SOCKET)
  if (NOT HAVE_SOCKET EQUAL 1)
    set(CMAKE_REQUIRED_LIBRARIES socket ${SOCKET_LIBRARIES})
    check_symbol_exists(socket "sys/types.h;sys/socket.h" HAVE_SOCKET2)
    if (HAVE_SOCKET2 EQUAL 1)
      set(SOCKET_LIBRARIES socket ${SOCKET_LIBRARIES})
    else ()
      set(CMAKE_REQUIRED_LIBRARIES socket nsl ${SOCKET_LIBRARIES})
      check_symbol_exists(socket "sys/types.h;sys/socket.h" HAVE_SOCKET3)
      if (HAVE_SOCKET3 EQUAL 1)
        set(SOCKET_LIBRARIES socket nsl ${SOCKET_LIBRARIES})
      else ()
        message(FATAL_ERROR "Cannot find socket library (containing symbol socket)")
      endif ()
    endif ()
  endif ()
endif ()
cmake_pop_check_state()

cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${SOCKET_LIBRARIES})
check_symbol_exists(poll poll.h HAVE_POLL)
if (NOT HAVE_POLL)
  if (WIN32)
    set(HAVE_SELECT 1)
  else()
    check_symbol_exists(select sys/select.h HAVE_SELECT)
  endif()
  if (NOT HAVE_SELECT)
    message(FATAL_ERROR "rabbitmq-c requires poll() or select() to be available")
  endif()
endif()
cmake_pop_check_state()

check_library_exists(rt clock_gettime "time.h" CLOCK_GETTIME_NEEDS_LIBRT)
check_library_exists(rt posix_spawnp "spawn.h" POSIX_SPAWNP_NEEDS_LIBRT)
if (CLOCK_GETTIME_NEEDS_LIBRT OR POSIX_SPAWNP_NEEDS_LIBRT)
  set(LIBRT rt)
endif()

option(ENABLE_SSL_SUPPORT "Enable SSL support" ON)

if (ENABLE_SSL_SUPPORT)
  find_package(OpenSSL 0.9.8 REQUIRED)

  cmake_push_check_state()
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  cmake_pop_check_state()
endif()

if (MSVC)
  set(CMAKE_C_FLAGS "/W4 /nologo ${CMAKE_C_FLAGS}")
elseif (CMAKE_C_COMPILER_ID MATCHES ".*Clang")
  set(CMAKE_C_FLAGS "-Wall -Wextra -Wstrict-prototypes -Wno-unused-function -fno-common -fvisibility=hidden ${CMAKE_C_FLAGS}")
elseif (CMAKE_COMPILER_IS_GNUCC)
  set(RMQ_C_FLAGS "-Wall -Wextra -Wstrict-prototypes -Wno-unused-function -fno-common")
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
  if (GCC_VERSION VERSION_GREATER 4.0 OR GCC_VERSION VERSION_EQUAL 4.0)
      set(RMQ_C_FLAGS "${RMQ_C_FLAGS} -fvisibility=hidden")
  endif()
  set(CMAKE_C_FLAGS "${RMQ_C_FLAGS} ${CMAKE_C_FLAGS}")
endif ()

CHECK_C_COMPILER_FLAG("-std=gnu90" HAVE_GNU90)
if (HAVE_GNU90)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu90")
else()
  CHECK_C_COMPILER_FLAG("-std=c90" HAVE_C90)
  if (HAVE_C90)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c90")
  endif()
endif()

option(REGENERATE_AMQP_FRAMING "Regenerate amqp_framing.h/amqp_framing.c sources (for developer use)" OFF)
mark_as_advanced(REGENERATE_AMQP_FRAMING)

if (REGENERATE_AMQP_FRAMING)
  find_package(PythonInterp)
  if (NOT PYTHONINTERP_FOUND)
    message(FATAL_ERROR "REGENERATE_AMQP_FRAMING requires Python to be available")
  endif ()

  #Determine Python Version:
  if(PYTHON_EXECUTABLE)
    execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c
      "import sys; sys.stdout.write(';'.join([str(x) for x in sys.version_info[:3]]))"
      OUTPUT_VARIABLE _VERSION
      RESULT_VARIABLE _PYTHON_VERSION_RESULT
      ERROR_QUIET)
    if(NOT _PYTHON_VERSION_RESULT)
      string(REPLACE ";" "." PYTHON_VERSION_STRING "${_VERSION}")
      list(GET _VERSION 0 PYTHON_VERSION_MAJOR)
      list(GET _VERSION 1 PYTHON_VERSION_MINOR)
      list(GET _VERSION 2 PYTHON_VERSION_PATCH)
      if(PYTHON_VERSION_PATCH EQUAL 0)
        # it's called "Python 2.7", not "2.7.0"
        string(REGEX REPLACE "\\.0$" "" PYTHON_VERSION_STRING "${PYTHON_VERSION_STRING}")
      endif()
    else()
      # sys.version predates sys.version_info, so use that
      execute_process(COMMAND "${PYTHON_EXECUTABLE}" -c "import sys; sys.stdout.write(sys.version)"
        OUTPUT_VARIABLE _VERSION
        RESULT_VARIABLE _PYTHON_VERSION_RESULT
        ERROR_QUIET)
      if(NOT _PYTHON_VERSION_RESULT)
        string(REGEX REPLACE " .*" "" PYTHON_VERSION_STRING "${_VERSION}")
        string(REGEX REPLACE "^([0-9]+)\\.[0-9]+.*" "\\1" PYTHON_VERSION_MAJOR "${PYTHON_VERSION_STRING}")
        string(REGEX REPLACE "^[0-9]+\\.([0-9])+.*" "\\1" PYTHON_VERSION_MINOR "${PYTHON_VERSION_STRING}")
        if(PYTHON_VERSION_STRING MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+.*")
          string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" PYTHON_VERSION_PATCH "${PYTHON_VERSION_STRING}")
        else()
          set(PYTHON_VERSION_PATCH "0")
        endif()
      else()
        # sys.version was first documented for Python 1.5, so assume
        # this is older.
        set(PYTHON_VERSION_STRING "1.4")
        set(PYTHON_VERSION_MAJOR "1")
        set(PYTHON_VERSION_MAJOR "4")
        set(PYTHON_VERSION_MAJOR "0")
      endif()
    endif()
    unset(_PYTHON_VERSION_RESULT)
    unset(_VERSION)
  endif(PYTHON_EXECUTABLE)

  # If we're running v3.x look for a 2to3 utility
  if (PYTHON_VERSION_MAJOR GREATER 2)
    get_filename_component(PYTHON_EXE_DIR ${PYTHON_EXECUTABLE} PATH)
    find_program(PYTHON_2TO3_EXECUTABLE
      NAMES 2to3
      HINTS ${PYTHON_EXE_DIR}
      )

    if ("PYTHON_2TO3_EXECUTABLE-NOTFOUND" STREQUAL PYTHON_2TO3_EXECUTABLE)
      message(FATAL_ERROR "Unable to find 2to3 python utility, specify python 2.7 or specify 2to3 utility")
    endif ()
  endif (PYTHON_VERSION_MAJOR GREATER 2)


  #check for json or simplejson
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -c "import json"
    RESULT_VARIABLE CHECK_PYTHON_JSON_FAILED
  )

  if (CHECK_PYTHON_JSON_FAILED)
    execute_process(COMMAND ${PYTHON_EXECUTABLE} -c "import simplejson"
      RESULT_VARIABLE CHECK_PYTHON_SIMPLEJSON_FAILED
    )

    if (CHECK_PYTHON_SIMPLEJSON_FAILED)
      message(FATAL_ERROR "REGENERATE_AMQP_FRAMING requires a python with json or simplejson modules")
    endif (CHECK_PYTHON_SIMPLEJSON_FAILED)
  endif (CHECK_PYTHON_JSON_FAILED)


  find_path(AMQP_CODEGEN_DIR
    amqp_codegen.py
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/codegen
          ${CMAKE_CURRENT_SOURCE_DIR}/rabbitmq-codegen
          ${CMAKE_CURRENT_SOURCE_DIR}/../rabbitmq-codegen
          DOC "Path to directory containing amqp_codegen.py (rabbitmq-codegen)"
          NO_DEFAULT_PATH
          )

  if (AMQP_CODEGEN_DIR STREQUAL "AMQP_CODEGEN_DIR-NOTFOUND")
    message(SEND_ERROR "REGENERATE_AMQP_FRAMING requires the amqp_codegen.py script. If this is a git clone you can:\n\ngit submodule init\ngit submodule update\n\n Or set AMQP_CODEGEN_DIR to directory containing amqp_codegen.py")
  else ()
    message(STATUS "Found amqp_codegen.py in ${AMQP_CODEGEN_DIR}")
  endif()
endif (REGENERATE_AMQP_FRAMING)

find_package(POPT)
find_package(XmlTo)
find_package(Doxygen)

if (POPT_FOUND AND XmlTo_FOUND)
  set(DO_DOCS ON)
endif()


option(BUILD_SHARED_LIBS "Build rabbitmq-c as a shared library" ON)
option(BUILD_STATIC_LIBS "Build rabbitmq-c as a static library" ON)

option(BUILD_EXAMPLES "Build Examples" ON)
option(BUILD_TOOLS "Build Tools (requires POPT Library)" ${POPT_FOUND})
option(BUILD_TOOLS_DOCS "Build man pages for Tools (requires xmlto)" ${DO_DOCS})
option(BUILD_TESTS "Build tests (run tests with make test)" ON)
option(BUILD_API_DOCS "Build Doxygen API docs" ${DOXYGEN_FOUND})

if (NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
    message(FATAL_ERROR "One or both of BUILD_SHARED_LIBS or BUILD_STATIC_LIBS must be set to ON to build")
endif()

add_subdirectory(librabbitmq)

if (BUILD_EXAMPLES)
  add_subdirectory(examples)
endif ()

if (BUILD_TOOLS)
  if (POPT_FOUND)
    add_subdirectory(tools)
  else ()
    message(WARNING "POpt library was not found. Tools will not be built")
  endif ()
endif ()

if (BUILD_TESTS)
  if (NOT BUILD_STATIC_LIBS)
    message(FATAL_ERROR
      "Tests can only be built against static libraries "
      "(set BUILD_STATIC_LIBS=ON)")
  endif ()
  enable_testing()
  add_subdirectory(tests)
endif (BUILD_TESTS)

if (BUILD_API_DOCS)
  if (NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is required to build the API documentation")
  endif ()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/docs/Doxyfile @ONLY)

  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE}
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs
    DEPENDS rabbitmq
    COMMENT "Generating API documentation"
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
    )
endif ()

set(libs_private ${SOCKET_LIBRARIES} ${LIBRT})
if (ENABLE_SSL_SUPPORT)
  set(requires_private "openssl")
  set(libs_private ${libs_private} ${CMAKE_THREAD_LIBS_INIT})
endif()

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")

configure_file(cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/librabbitmq/config.h)
configure_file(librabbitmq.pc.in ${CMAKE_CURRENT_BINARY_DIR}/librabbitmq.pc @ONLY)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/librabbitmq.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )

if (BUILD_SHARED_LIBS)
  message(STATUS "Building rabbitmq as a shared library - yes")
else ()
  message(STATUS "Building rabbitmq as a shared library - no")
endif ()

if (BUILD_STATIC_LIBS)
  message(STATUS "Building rabbitmq as a static library - yes")
else ()
  message(STATUS "Building rabbitmq as a static library - no")
endif ()
