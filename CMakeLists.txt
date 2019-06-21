# CMake build script for ZeroTier One

cmake_minimum_required (VERSION 3.8)
project (zerotier-one)

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

if(CMAKE_BUILD_TYPE EQUAL "Debug")
	add_definitions(-DZT_TRACE)
endif(CMAKE_BUILD_TYPE EQUAL "Debug")

if(WIN32)
	add_definitions(-DNOMINMAX)
	if(CMAKE_BUILD_TYPE EQUAL "Debug")
		add_definitions(-DZT_WIN_RUN_IN_CONSOLE)
	endif(CMAKE_BUILD_TYPE EQUAL "Debug")
endif(WIN32)


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
	zt_controller
	zt_core
	zt_osdep
	zt_service
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
endif(WIN32)

if(BUILD_CENTRAL_CONTROLLER)
	set(libs ${libs} rabbitmq-static ${PostgreSQL_LIBRARIES})
endif(BUILD_CENTRAL_CONTROLLER)

add_executable(${PROJECT_NAME} ${src} ${headers})
target_link_libraries(${PROJECT_NAME} ${libs})