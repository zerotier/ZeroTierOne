cmake_minimum_required(VERSION 2.8)
project(zt_controller)

if(WIN32)
	add_definitions(-DNOMINMAX)
endif(WIN32)

set(ctl_src
	DB.cpp
	DBMirrorSet.cpp
	EmbeddedNetworkController.cpp
	FileDB.cpp
	LFDB.cpp
	RabbitMQ.cpp
)

set(ctl_hdr
	DB.hpp
	DBMirrorSet.hpp
	EmbeddedNetworkController.hpp
	FileDB.hpp
	LFDB.hpp
	RabbitMQ.hpp
)

if(BUILD_CENTRAL_CONTROLLER)
	add_definitions(-DZT_CONTROLLER_USE_LIBPQ)
	include_directories("../ext/librabbitmq/librabbitmq" ${PostgreSQL_INCLUDE_DIRS})

	set(ctl_src ${ctl_src} PostgreSQL.cpp)
	set(ctl_hdr ${ctl_hdr} PostgreSQL.hpp)
endif(BUILD_CENTRAL_CONTROLLER)

add_library(${PROJECT_NAME} STATIC ${ctl_src} ${ctl_hdr})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

