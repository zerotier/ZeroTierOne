cmake_minimum_required (VERSION 3.8)
project(zerotier DESCRIPTION "ZeroTier Network Hypervisor" LANGUAGES CXX C)

set(ZEROTIER_VERSION_MAJOR 1 CACHE INTERNAL "")
set(ZEROTIER_VERSION_MINOR 9 CACHE INTERNAL "")
set(ZEROTIER_VERSION_REVISION 0 CACHE INTERNAL "")
set(ZEROTIER_VERSION_BUILD 0 CACHE INTERNAL "")

if(${CMAKE_VERSION} VERSION_LESS 3.15)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
	cmake_policy(VERSION 3.15)
endif()

find_program(
	GO go
	HINTS "/usr/local/go/bin" "C:/go/bin"
)

if(NOT GO)
	message(FATAL_ERROR "Golang not found")
else(NOT GO)
	message(STATUS "Found Golang at ${GO}")
endif(NOT GO)


set(CMAKE_CXX_STANDARD 11)
set(default_build_type "Release")

if(WIN32)
	set(CMAKE_SYSTEM_VERSION "7" CACHE STRING INTERNAL FORCE)
endif(WIN32)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "Minimum OS X Deployment Version")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option(BUILD_CENTRAL_CONTROLLER "Build ZeroTier Central Controller" OFF)
if(BUILD_CENTRAL_CONTROLLER)
	find_package(PkgConfig REQUIRED)
	if(APPLE)
		set(CMAKE_PREFIX_PATH 
			${CMAKE_PREFIX_PATH}
			/usr/local/opt/libpq
			/usr/local/lib
		)
	endif(APPLE)
	find_package(PostgreSQL REQUIRED)
	
	pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
	
	add_subdirectory(controller/thirdparty/redis-plus-plus-1.1.1)
	set(redispp_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/controller/thirdparty/redis-plus-plus-1.1.1/src/sw)
	set(redispp_STATIC_LIB redispp_static)
endif(BUILD_CENTRAL_CONTROLLER)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DZT_DEBUG)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

if(WIN32)
	message("++ Setting Windows Compiler Flags ${CMAKE_BUILD_TYPE}")
	add_definitions(-DNOMINMAX)
else(WIN32)
	if(APPLE)
		message("++ Setting MacOS Compiler Flags ${CMAKE_BUILD_TYPE}")

		add_compile_options(
			-Wall
			-Wno-deprecated
			-Wno-unused-function
			-mmacosx-version-min=10.12
			$<$<CONFIG:DEBUG>:-g>
			$<$<CONFIG:DEBUG>:-O0>
			$<$<CONFIG:RELEASE>:-Ofast>
			$<$<CONFIG:RELEASE>:-ffast-math>
			$<$<CONFIG:RELEASE>:-fPIE>
			$<$<CONFIG:RELEASE>:-flto>
			$<$<CONFIG:RELWITHDEBINFO>:-Ofast>
			$<$<CONFIG:RELWITHDEBINFO>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-g>
		)
		add_link_options(
			-mmacosx-version-min=10.12
			$<$<CONFIG:RELEASE>:-flto>
		)
	else(APPLE)
		message("++ Setting Linux/BSD/Posix Compiler Flags (${CMAKE_BUILD_TYPE})")
		add_compile_options(
			-Wall
			-Wno-deprecated
			-Wno-unused-function
			-Wno-format
			$<$<CONFIG:DEBUG>:-g>
			$<$<CONFIG:DEBUG>:-O0>
			$<$<CONFIG:RELEASE>:-O3>
			$<$<CONFIG:RELEASE>:-ffast-math>
			$<$<CONFIG:RELEASE>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-O3>
			$<$<CONFIG:RELWITHDEBINFO>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-g>
		)
		option(BUILD_32BIT "Force building as 32-bit binary")
		if(BUILD_32BIT)
			set(CMAKE_SYSTEM_PROCESSOR "x86" CACHE STRING "system processor")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "c++ flags")
			set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32" CACHE STRING "c flags")
		endif(BUILD_32BIT)
	endif(APPLE)
endif(WIN32)

if (
	CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "amd64"
)
	message("++ Adding flags for processor ${CMAKE_SYSTEM_PROCESSOR}")
	add_compile_options(-maes -mrdrnd -mpclmul -msse -msse2 -mssse3)
endif()

set(GO_BUILD_TAGS)
if(BUILD_CENTRAL_CONTROLLER)
	add_definitions(-DZT_CONTROLLER_USE_LIBPQ=1)
	set(GO_BUILD_TAGS -tags central)
endif(BUILD_CENTRAL_CONTROLLER)

add_subdirectory(core)
add_subdirectory(controller)
add_subdirectory(osdep)
add_subdirectory(serviceiocore)

file(GLOB go_src 
	${CMAKE_SOURCE_DIR}/cmd/*.go
	${CMAKE_SOURCE_DIR}/cmd/cmd/*.go
	${CMAKE_SOURCE_DIR}/pkg/zerotier/*.go)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/zerotier_cgo.h ${CMAKE_BINARY_DIR}/zerotier_cgo.a
	COMMAND ${GO} build -buildmode=c-archive -o ${CMAKE_BINARY_DIR}/zerotier_cgo.a ${CMAKE_SOURCE_DIR}/cmd/zerotier/zerotier.go
	IMPLICIT_DEPENDS ${go_src}
	COMMENT "Compiling Go Code..."
)
add_custom_target(
	zerotier_cgo_target 
	DEPENDS ${CMAKE_BINARY_DIR}/zerotier_cgo.a
	SOURCES ${go_src}
)
add_library(zerotier_cgo STATIC IMPORTED GLOBAL)
add_dependencies(zerotier_cgo zerotier_cgo_target)
set_target_properties(
	zerotier_cgo
	PROPERTIES
	IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/zerotier_cgo.a
	INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
)

add_executable(zerotier main.cpp)
target_include_directories(zerotier PUBLIC ${CMAKE_BINARY_DIR})
add_dependencies(zerotier zerotier_cgo zt_osdep zt_core zt_controller zt_service_io_core)
target_link_libraries(zerotier zerotier_cgo zt_service_io_core zt_core zt_osdep zt_controller )
if (APPLE)
	target_link_libraries(zerotier "-framework CoreFoundation" "-framework Security")
else(APPLE)
	if ("${CMAKE_SYSTEM}" MATCHES "Linux")
		target_link_libraries(zerotier "-lpthread" "-lm")
	endif ("${CMAKE_SYSTEM}" MATCHES "Linux")
endif (APPLE)
