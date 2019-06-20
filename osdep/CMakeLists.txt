cmake_minimum_required(VERSION 2.8)
project(zt_osdep)

set(src
	Arp.cpp
	Http.cpp
	ManagedRoute.cpp
	NeighborDiscovery.cpp
	OSUtils.cpp
	PortMapper.cpp
)

set(headers
	Arp.hpp
	Binder.hpp
	BlockingQueue.hpp
	Http.hpp
	ManagedRoute.hpp
	OSUtils.hpp
	Phy.hpp
	PortMapper.hpp
	Thread.hpp
)

if(WIN32)
	set(src ${src} WindowsEthernetTap.cpp)
	set(headers ${headers} WindowsEthernetTap.hpp)
elseif(UNIX)
	if(APPLE)
		set(src ${src} MacEthernetTap.cpp)
		set(headers ${headers} MacEthernetTap.hpp)
	endif(APPLE)

	if(CMAKE_SYSTEM_NAME MATCHES "BSD")
		set(src ${src} BSDEthernetTap.cpp)
		set(headers ${headers} BSDEthernetTap.hpp)
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
		set(src ${src} freebsd_getifmaddrs.c)
		set(headers ${headers} freebsd_getifmaddrs.h)
	endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

	if(CMAKE_SYSTEM_NAME EQUAL "Linux")
		set(src ${src} LinuxEthernetTap.cpp)
		set(headers ${headers} LinuxEthernetTap.hpp)
	endif(CMAKE_SYSTEM_NAME EQUAL "Linux")
endif(WIN32)

add_library(${PROJECT_NAME} STATIC ${src} ${headers})
