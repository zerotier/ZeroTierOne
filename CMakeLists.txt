cmake_minimum_required (VERSION 3.8)
project(zerotier DESCRIPTION "ZeroTier Network Hypervisor" LANGUAGES CXX C)

if(${CMAKE_VERSION} VERSION_LESS 3.15)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
	cmake_policy(VERSION 3.15)
endif()

if(WIN32)
	set(CMAKE_SYSTEM_VERSION "7" CACHE STRING INTERNAL FORCE)
endif(WIN32)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X Deployment Version")

set(ZEROTIER_ONE_VERSION_MAJOR 2 CACHE INTERNAL "")
set(ZEROTIER_ONE_VERSION_MINOR 0 CACHE INTERNAL "")
set(ZEROTIER_ONE_VERSION_REVISION 0 CACHE INTERNAL "")
set(ZEROTIER_ONE_VERSION_BUILD 0 CACHE INTERNAL "")

configure_file(
	${CMAKE_SOURCE_DIR}/version.h.in
	${CMAKE_BINARY_DIR}/version.h
)

set(default_build_type "Release")
#if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
#	set(default_build_type "Debug")
#endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option(BUILD_CENTRAL_CONTROLLER "Build ZeroTier Central Controller" OFF)
if (BUILD_CENTRAL_CONTROLLER)
	find_package(PostgreSQL REQUIRED)
	set(ENABLE_SSL_SUPPORT OFF)
	set(BUILD_SHARED_LIBS OFF)
	set(BUILD_EXAMPLES OFF)
	set(BUILD_TOOLS OFF)
	set(BUILD_TESTS OFF)
	set(BUILD_API_DOCS OFF)
	add_subdirectory("ext/librabbitmq")
endif(BUILD_CENTRAL_CONTROLLER)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DZT_TRACE)
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
			-mmacosx-version-min=10.9
			$<$<CONFIG:Debug>:-g>
			$<$<CONFIG:DEBUG>:-O0>
			$<$<CONFIG:RELEASE>:-Ofast>
			$<$<CONFIG:RELEASE>:-fPIE>
			$<$<CONFIG:RELEASE>:-flto>
			$<$<CONFIG:RELWITHDEBINFO>:-Ofast>
			$<$<CONFIG:RELWITHDEBINFO>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-g>
		)
		add_link_options(
			-mmacosx-version-min=10.9
			$<$<CONFIG:RELEASE>:-flto>
		)

	else(APPLE)

		message("++ Setting Linux/BSD/Posix Compiler Flags (${CMAKE_BUILD_TYPE})")
		add_compile_options(
			-Wall
			-Wno-deprecated
			-Wno-unused-function
			$<$<CONFIG:Debug>:-g>
			$<$<CONFIG:DEBUG>:-O0>
			$<$<CONFIG:RELEASE>:-O3>
			$<$<CONFIG:RELEASE>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-O3>
			$<$<CONFIG:RELWITHDEBINFO>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-g>
		)

	endif(APPLE)
endif(WIN32)

if (
	CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "amd64" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "i386" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "i486" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "i586" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "i686"
)
	message("++ Adding SSE and AES-NI flags for processor ${CMAKE_SYSTEM_PROCESSOR}")
	add_compile_options(
		-maes
		-mrdrnd
		-mpclmul
		-msse
		-msse2
		-msse3
		-msse4.1
	)
endif()

if(ZT_TRACE)
	add_definitions(-DZT_TRACE)
endif()

add_subdirectory(node)
add_subdirectory(controller)
add_subdirectory(osdep)
add_subdirectory(root)
add_subdirectory(go/native)

set(
	zt_osdep
	zt_core
	zt_controller
	zt_go_native
)

#if(WIN32)
#	set(libs ${libs} wsock32 ws2_32 rpcrt4 iphlpapi)
#else(WIN32)
#	set(libs ${libs} pthread)
#endif(WIN32)

#if(BUILD_CENTRAL_CONTROLLER)
#	set(libs ${libs} rabbitmq-static ${PostgreSQL_LIBRARIES})
#endif(BUILD_CENTRAL_CONTROLLER)

add_custom_target(zerotier ALL
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/go
	COMMAND rm -f ../build/zerotier && go build -trimpath -ldflags -s -o ../build/zerotier cmd/zerotier/zerotier.go
)
add_dependencies(zerotier zt_osdep zt_core zt_go_native)
