cmake_minimum_required(VERSION 3.0)
project(zt_osdep)

set(src
	ManagedRoute.cpp
	OSUtils.cpp
	rust-osdep.cpp
)

set(headers
	ManagedRoute.hpp
	OSUtils.hpp
	Thread.hpp
	rust-osdep.h
)

if(WIN32)
	set(src ${src} WindowsEthernetTap.cpp)
	set(headers ${headers} WindowsEthernetTap.hpp)
elseif(UNIX)
	if(CMAKE_SYSTEM_NAME MATCHES "BSD")
		set(src ${src} BSDEthernetTap.cpp)
		set(headers ${headers} BSDEthernetTap.hpp)
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
		set(src ${src} freebsd_getifmaddrs.c)
		set(headers ${headers} freebsd_getifmaddrs.h)
	endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
endif(WIN32)

add_library(${PROJECT_NAME} STATIC ${src} ${headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
target_include_directories(
	${PROJECT_NAME}
	PUBLIC
	${CMAKE_BINARY_DIR}/core
)
