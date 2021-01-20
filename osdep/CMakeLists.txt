cmake_minimum_required(VERSION 3.0)
project(zt_osdep)

set(src
	Arp.cpp
	EthernetTap.cpp
	ManagedRoute.cpp
	NeighborDiscovery.cpp
	OSUtils.cpp
)

set(headers
	Arp.hpp
	BlockingQueue.hpp
	EthernetTap.hpp
	ManagedRoute.hpp
	OSUtils.hpp
	Thread.hpp
)

if(WIN32)
	set(src ${src} WindowsEthernetTap.cpp)
	set(headers ${headers} WindowsEthernetTap.hpp)
elseif(UNIX)
	if(APPLE)
		set(src ${src} MacEthernetTap.cpp MacKextEthernetTap.cpp)
		set(headers ${headers} MacEthernetTap.hpp MacKextEthernetTap.hpp)
	endif(APPLE)

	if(CMAKE_SYSTEM_NAME MATCHES "BSD")
		set(src ${src} BSDEthernetTap.cpp)
		set(headers ${headers} BSDEthernetTap.hpp)
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
		set(src ${src} freebsd_getifmaddrs.c)
		set(headers ${headers} freebsd_getifmaddrs.h)
	endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		set(src ${src} LinuxEthernetTap.cpp LinuxNetLink.cpp)
		set(headers ${headers} LinuxEthernetTap.hpp LinuxNetLink.hpp)
	endif(CMAKE_SYSTEM_NAME MATCHES "Linux")
endif(WIN32)

add_library(${PROJECT_NAME} STATIC ${src} ${headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
target_include_directories(
	${PROJECT_NAME}
	PUBLIC
	${CMAKE_BINARY_DIR}/core
)

if(APPLE)
	add_executable(MacEthernetTapAgent MacEthernetTapAgent.c MacEthernetTapAgent.h)
	target_include_directories(MacEthernetTapAgent PRIVATE ${CMAKE_BINARY_DIR})
	target_include_directories(
		MacEthernetTapAgent
		PUBLIC
		${CMAKE_BINARY_DIR}/core
	)
endif(APPLE)
