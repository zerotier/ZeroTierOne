function(detect_code_compiled code macro msg)
    message(STATUS "Detecting ${msg}")
    check_cxx_source_compiles("${code}" "${macro}" FAIL_REGEX "warning")
    if(${macro})
        message(STATUS "Detecting ${msg} - supported")
    else()
        message(STATUS "Detecting ${msg} - not supported")
    endif()
endfunction(detect_code_compiled)

include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CMakeDetermineCompileFeatures)
include(CheckCXXSourceCompiles)
include(CMakeFindDependencyMacro)

if(NOT PostgreSQL_FOUND)
    if(POLICY CMP0074)
        cmake_policy(PUSH)
        # CMP0074 is `OLD` by `cmake_minimum_required(VERSION 3.7)`,
        # sets `NEW` to enable support CMake variable `PostgreSQL_ROOT`.
        cmake_policy(SET CMP0074 NEW)
    endif()

    find_package(PostgreSQL REQUIRED)

    if(POLICY CMP0074)
        cmake_policy(POP)
    endif()
endif()

check_function_exists("poll" PQXX_HAVE_POLL)

set(CMAKE_REQUIRED_LIBRARIES pq)
check_symbol_exists(
	PQencryptPasswordConn
	"${PostgreSQL_INCLUDE_DIR}/libpq-fe.h"
	PQXX_HAVE_PQENCRYPTPASSWORDCONN)
check_symbol_exists(
	PQenterPipelineMode
	"${PostgreSQL_INCLUDE_DIR}/libpq-fe.h"
	PQXX_HAVE_PQ_PIPELINE)

cmake_determine_compile_features(CXX)
cmake_policy(SET CMP0057 NEW)

# check_cxx_source_compiles requires CMAKE_REQUIRED_DEFINITIONS to specify
# compiling arguments.
# Wordaround: Push CMAKE_REQUIRED_DEFINITIONS
if(CMAKE_REQUIRED_DEFINITIONS)
    set(def "${CMAKE_REQUIRED_DEFINITIONS}")
endif()
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_CXX${CMAKE_CXX_STANDARD}_STANDARD_COMPILE_OPTION})
set(CMAKE_REQUIRED_QUIET ON)

try_compile(
	PQXX_HAVE_GCC_PURE
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/gcc_pure.cxx)
try_compile(
	PQXX_HAVE_GCC_VISIBILITY
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/gcc_visibility.cxx)
try_compile(
	PQXX_HAVE_LIKELY
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/likely.cxx)
try_compile(
	PQXX_HAVE_CXA_DEMANGLE
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/cxa_demangle.cxx)
try_compile(
	PQXX_HAVE_CONCEPTS
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/concepts.cxx)
try_compile(
	PQXX_HAVE_SPAN
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/span.cxx)
try_compile(
	PQXX_HAVE_MULTIDIMENSIONAL_SUBSCRIPT
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/multidim-subscript.cxx)
try_compile(
	PQXX_HAVE_CHARCONV_FLOAT
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/charconv_float.cxx)
try_compile(
	PQXX_HAVE_CHARCONV_INT
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/charconv_int.cxx)
try_compile(
	PQXX_HAVE_PATH
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/fs.cxx)
try_compile(
	PQXX_HAVE_THREAD_LOCAL
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/thread_local.cxx)
try_compile(
	PQXX_HAVE_SLEEP_FOR
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/sleep_for.cxx)
try_compile(
	PQXX_HAVE_STRERROR_R
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/strerror_r.cxx)
try_compile(
	PQXX_HAVE_STRERROR_S
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/strerror_s.cxx)
try_compile(
	PQXX_HAVE_YEAR_MONTH_DAY
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/year_month_day.cxx)
try_compile(
	PQXX_HAVE_CMP
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/cmp.cxx)

try_compile(
	need_fslib
	${PROJECT_BINARY_DIR}
	SOURCES ${PROJECT_SOURCE_DIR}/config-tests/need_fslib.cxx)
if(!need_fslib)
    # TODO: This may work for gcc 8, but some clang versions may need -lc++fs.
    link_libraries(stdc++fs)
endif()

# check_cxx_source_compiles requires CMAKE_REQUIRED_DEFINITIONS to specify
# compiling arguments.
# Workaround: Pop CMAKE_REQUIRED_DEFINITIONS
if(def)
    set(CMAKE_REQUIRED_DEFINITIONS ${def})
    unset(def CACHE)
else()
    unset(CMAKE_REQUIRED_DEFINITIONS CACHE)
endif()
set(CMAKE_REQUIRED_QUIET OFF)

set(AC_CONFIG_H_IN "${PROJECT_SOURCE_DIR}/include/pqxx/config.h.in")
set(CM_CONFIG_H_IN "${PROJECT_BINARY_DIR}/include/pqxx/config_cmake.h.in")
set(CM_CONFIG_PUB "${PROJECT_BINARY_DIR}/include/pqxx/config-public-compiler.h")
set(CM_CONFIG_INT "${PROJECT_BINARY_DIR}/include/pqxx/config-internal-compiler.h")
set(CM_CONFIG_PQ "${PROJECT_BINARY_DIR}/include/pqxx/config-internal-libpq.h")
message(STATUS "Generating config.h")
file(WRITE "${CM_CONFIG_H_IN}" "")
file(STRINGS "${AC_CONFIG_H_IN}" lines)
foreach(line ${lines})
    string(REGEX REPLACE "^#undef" "#cmakedefine" l "${line}")
    file(APPEND "${CM_CONFIG_H_IN}" "${l}\n")
endforeach()
configure_file("${CM_CONFIG_H_IN}" "${CM_CONFIG_INT}" @ONLY)
configure_file("${CM_CONFIG_H_IN}" "${CM_CONFIG_PUB}" @ONLY)
configure_file("${CM_CONFIG_H_IN}" "${CM_CONFIG_PQ}" @ONLY)
message(STATUS "Generating config.h - done")
