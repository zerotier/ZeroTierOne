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
)

set(ctl_hdr
	DB.hpp
	DBMirrorSet.hpp
	EmbeddedNetworkController.hpp
	FileDB.hpp
	LFDB.hpp
	Redis.hpp
)

if(BUILD_CENTRAL_CONTROLLER)
	set(ctl_src ${ctl_src} PostgreSQL.cpp)
	set(ctl_hdr ${ctl_hdr} PostgreSQL.hpp)
endif(BUILD_CENTRAL_CONTROLLER)

add_library(${PROJECT_NAME} STATIC ${ctl_src} ${ctl_hdr} ${PostgreSQL_LIBRARIES} ${STATIC_LIB} ${hiredis_libraries})
target_include_directories(${PROJECT_NAME}
	PUBLIC ${hiredis_INCLUDE_DIRS}/../ ${CMAKE_CURRENT_LIST_DIR}/thirdparty/redis-plus-plus-1.1.1/src/sw ${PostgreSQL_INCLUDE_DIRS}
	PRIVATE ${CMAKE_BINARY_DIR}/core)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

