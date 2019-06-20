cmake_minimum_required(VERSION 2.8)
project(zt_controller)

if(WIN32)
	add_definitions(-DNOMINMAX)
	if(CMAKE_BUILD_TYPE EQUAL "Debug")
		
	endif(CMAKE_BUILD_TYPE EQUAL "Debug")
endif(WIN32)

set(ctl_src
	DB.cpp
	EmbeddedNetworkController.cpp
	FileDB.cpp
	RabbitMQ.cpp
)

set(ctl_hdr
	DB.hpp
	EmbeddedNetworkController.hpp
	FileDB.hpp
	RabbitMQ.hpp
)

if(BUILD_CENTRAL_CONTROLLER)
	add_definitions(-DZT_CONTROLLER_USE_LIBPQ)
	include_directories("../ext/librabbitmq/librabbitmq" ${PostgreSQL_INCLUDE_DIRS})

	set(ctl_src ${ctl_src} PostgreSQL.cpp)
	set(ctl_hdr ${ctl_hdr} PostgreSQL.hpp)
endif(BUILD_CENTRAL_CONTROLLER)

add_library(${PROJECT_NAME} STATIC ${ctl_src} ${ctl_hdr})