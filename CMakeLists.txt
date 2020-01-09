cmake_minimum_required (VERSION 3.8)

if(${CMAKE_VERSION} VERSION_LESS 3.15)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
	cmake_policy(VERSION 3.15)
endif()

if(WIN32)
	# If building on Windows, set minimum target to Windows 7
	set(CMAKE_SYSTEM_VERSION "7" CACHE STRING INTERNAL FORCE)
endif(WIN32)

set(ZEROTIER_ONE_VERSION_MAJOR 2 CACHE INTERNAL "")
set(ZEROTIER_ONE_VERSION_MINOR 0 CACHE INTERNAL "")
set(ZEROTIER_ONE_VERSION_REVISION 0 CACHE INTERNAL "")
set(ZEROTIER_ONE_VERSION_BUILD 0 CACHE INTERNAL "")

set(default_build_type "Release")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
	set(default_build_type "Debug")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
		STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
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

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X Deployment Version")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DZT_TRACE)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

project(zerotier
	DESCRIPTION "ZeroTier Network Hypervisor"
	LANGUAGES CXX C)

if(WIN32)
	add_definitions(-DNOMINMAX)
else(WIN32)
	if(APPLE)

		message("Setting macOS Compiler Flags ${CMAKE_BUILD_TYPE}")
		add_compile_options(
			-Wall
			-Wno-deprecated
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

	elseif (
		CMAKE_SYSTEM_NAME MATCHES "Linux" OR
		CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR
		CMAKE_SYSTEM_NAME MATCHES "OpenBSD" OR
		CMAKE_SYSTEM_NAME MATCHES "NetBSD"
	)

		message("Setting Linux/BSD Compiler Flags (${CMAKE_BUILD_TYPE})")
		add_compile_options(
			-Wall
			-Wno-deprecated
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
	message("Adding SSE and AES-NI flags for processor ${CMAKE_SYSTEM_PROCESSOR}")
	add_compile_options(
		-maes
		-mmmx
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
if(ZT_DEBUG_TRACE)
	add_definitions(-DZT_DEBUG_TRACE)
endif()

add_subdirectory(node)
add_subdirectory(controller)
add_subdirectory(osdep)
add_subdirectory(root)
add_subdirectory(go/native)

#if(WIN32)
#	add_subdirectory("windows/WinUI")
#	add_subdirectory("windows/copyutil")
#	add_definitions(-DNOMINMAX)
#endif(WIN32)

set(
	zt_osdep
	zt_core
	zt_controller
	zt_go_native
)

configure_file(
	${CMAKE_SOURCE_DIR}/version.h.in
	${CMAKE_BINARY_DIR}/version.h
)

#set(src
#	one.cpp
#	"ext/http-parser/http_parser.c"
#)
#set(headers
#	"ext/http-parser/http_parser.h"
#)

if(WIN32)
	set(libs ${libs} wsock32 ws2_32 rpcrt4 iphlpapi)
else(WIN32)
	set(libs ${libs} pthread)
endif(WIN32)

#if(WIN32)
#	set(libs ${libs} wsock32 ws2_32 rpcrt4 iphlpapi)
#	set(src
#		${src}
#		"windows/ZeroTierOne/ServiceBase.cpp"
#		"windows/ZeroTierOne/ServiceInstaller.cpp"
#		"windows/ZeroTierOne/ZeroTierOneService.cpp"
#		"windows/ZeroTierOne/ZeroTierOne.rc"
#	)
#	set(headers
#		${headers}
#		"windows/ZeroTierOne/ServiceBase.h"
#		"windows/ZeroTierOne/ServiceInstaller.h"
#		"windows/ZeroTierOne/ZeroTierOneService.h"
#	)
#else(WIN32)
#	set(libs ${libs} pthread resolv)
#endif(WIN32)

#if(BUILD_CENTRAL_CONTROLLER)
#	set(libs ${libs} rabbitmq-static ${PostgreSQL_LIBRARIES})
#endif(BUILD_CENTRAL_CONTROLLER)

#add_executable(${PROJECT_NAME} ${src} ${headers})
#target_link_libraries(${PROJECT_NAME} ${libs})
#target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

add_custom_command(
	OUTPUT zerotier
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/go
	COMMAND go build -trimpath -ldflags -s -o ../build/zerotier cmd/zerotier/zerotier.go
	DEPENDS zt_osdep zt_core zt_go_native
)
add_custom_target(build_zerotier ALL DEPENDS zerotier)

add_executable(zerotier-selftest selftest.cpp)
target_link_libraries(zerotier-selftest ${libs} zt_core zt_osdep)
target_compile_features(zerotier-selftest PUBLIC cxx_std_11)
