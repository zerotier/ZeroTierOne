# CMake build script for ZeroTier One

cmake_minimum_required (VERSION 3.8)

# Set a default build type if none was specified
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

if(WIN32)
	add_definitions(-DNOMINMAX)
else(WIN32)
	if(APPLE)

	elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
		message("Setting Linux Compiler Flags ${CMAKE_BUILD_TYPE}")
		add_compile_options(
			-Wall
			-Wno-deprecated
			$<$<CONFIG:Debug>:-g>
			$<$<CONFIG:DEBUG>:-O0>
			$<$<CONFIG:RELEASE>:-O3>
			$<$<CONFIG:RELEASE>:-fstackprotector>
			$<$<CONFIG:RELEASE>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-O3>
			$<$<CONFIG:RELWITHDEBINFO>:-fstackprotector>
			$<$<CONFIG:RELWITHDEBINFO>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-g>
		)
	endif(APPLE)
endif(WIN32)

project(zerotier-one)

add_subdirectory(node)
add_subdirectory(controller)
add_subdirectory(osdep)
add_subdirectory(service)

if(WIN32)
	add_subdirectory("windows/WinUI")
	add_subdirectory("windows/copyutil")
	add_definitions(-DNOMINMAX)
endif(WIN32)

set(libs
	zt_service
	zt_osdep
	zt_core
	zt_controller
)

set(src
	one.cpp
	"ext/http-parser/http_parser.c"
)
set(headers
	"ext/http-parser/http_parser.h"
)

if(WIN32)
	set(libs ${libs} wsock32 ws2_32 rpcrt4 iphlpapi)
	set(src
		${src}
		"windows/ZeroTierOne/ServiceBase.cpp"
		"windows/ZeroTierOne/ServiceInstaller.cpp"
		"windows/ZeroTierOne/ZeroTierOneService.cpp"
		"windows/ZeroTierOne/ZeroTierOne.rc"
	)
	set(headers
		${headers}
		"windows/ZeroTierOne/ServiceBase.h"
		"windows/ZeroTierOne/ServiceInstaller.h"
		"windows/ZeroTierOne/ZeroTierOneService.h"
	)
else(WIN32)
	set(libs ${libs} pthread)
endif(WIN32)

if(BUILD_CENTRAL_CONTROLLER)
	set(libs ${libs} rabbitmq-static ${PostgreSQL_LIBRARIES})
endif(BUILD_CENTRAL_CONTROLLER)

add_executable(${PROJECT_NAME} ${src} ${headers})
target_link_libraries(${PROJECT_NAME} ${libs})
