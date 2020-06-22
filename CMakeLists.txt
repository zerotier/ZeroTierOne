cmake_minimum_required (VERSION 3.8)

cmake_policy(SET CMP0048 NEW)
if(${CMAKE_VERSION} VERSION_LESS 3.15)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
	cmake_policy(VERSION 3.15)
endif()

set(ZEROTIER_VERSION_MAJOR 1 CACHE INTERNAL "")
set(ZEROTIER_VERSION_MINOR 9 CACHE INTERNAL "")
set(ZEROTIER_VERSION_REVISION 0 CACHE INTERNAL "")
set(ZEROTIER_VERSION_BUILD 0 CACHE INTERNAL "")

project(zerotier
	VERSION ${ZEROTIER_VERSION_MAJOR}.${ZEROTIER_VERSION_MINOR}.${ZEROTIER_VERSION_REVISION}.${ZEROTIER_VERSION_BUILD}
	DESCRIPTION "ZeroTier Network Hypervisor" 
	LANGUAGES CXX C)


if(${CMAKE_VERSION} VERSION_LESS 3.15)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
	cmake_policy(VERSION 3.15)
endif()

find_program(
	GO go
	HINTS "/usr/local/go/bin" "C:/go/bin"
)

if(NOT GO)
	message(FATAL_ERROR "Golang not found")
else(NOT GO)
	message(STATUS "Found Golang at ${GO}")
endif(NOT GO)

set(CMAKE_CXX_STANDARD 11)
set(default_build_type "Release")

if(WIN32)
	set(CMAKE_SYSTEM_VERSION "7" CACHE STRING INTERNAL FORCE)
endif(WIN32)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "Minimum OS X Deployment Version")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option(BUILD_CENTRAL_CONTROLLER "Build ZeroTier Central Controller" OFF)
if(BUILD_CENTRAL_CONTROLLER)
	find_package(PkgConfig REQUIRED)
	if(APPLE)
		set(CMAKE_PREFIX_PATH 
			${CMAKE_PREFIX_PATH}
			/usr/local/opt/libpq
			/usr/local/lib
		)
	endif(APPLE)
	find_package(PostgreSQL REQUIRED)
	
	pkg_check_modules(hiredis REQUIRED IMPORTED_TARGET hiredis)
	
	add_subdirectory(controller/thirdparty/redis-plus-plus-1.1.1)
	set(redispp_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/controller/thirdparty/redis-plus-plus-1.1.1/src/sw)
	set(redispp_STATIC_LIB redispp_static)
endif(BUILD_CENTRAL_CONTROLLER)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DZT_DEBUG)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

if(WIN32)

	message("++ Setting Windows Compiler Flags ${CMAKE_BUILD_TYPE}")
	add_definitions(-DNOMINMAX)

	set(GOFLAGS
		-trimpath
	)

	option(BUILD_32BIT "Force building as 32-bit binary" OFF)

	if(BUILD_32BIT)
		set(CMAKE_SYSTEM_PROCESSOR "x86" CACHE STRING "system processor")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "c++ flags")
		set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32" CACHE STRING "c flags")
		set(GOARCH "GOARCH=386" CACHE STRING "go architecture")
		add_compile_options(
			-m32
		)
		add_link_options(
			-m32
		)
	endif(BUILD_32BIT)
else(WIN32)

	set(GOFLAGS
		-trimpath
		-buildmode=pie
	)

	if(APPLE)

		message("++ Setting MacOS Compiler Flags ${CMAKE_BUILD_TYPE}")

		add_compile_options(
			-Wall
			-Wno-deprecated
			-Wno-unused-function
			-mmacosx-version-min=10.12
			$<$<CONFIG:DEBUG>:-g>
			$<$<CONFIG:DEBUG>:-O0>
			$<$<CONFIG:RELEASE>:-Ofast>
			$<$<CONFIG:RELEASE>:-ffast-math>
			$<$<CONFIG:RELEASE>:-fPIE>
			$<$<CONFIG:RELEASE>:-flto>
			$<$<CONFIG:RELWITHDEBINFO>:-O1>
			$<$<CONFIG:RELWITHDEBINFO>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-g>
		)

		add_link_options(
			-mmacosx-version-min=10.12
			$<$<CONFIG:RELEASE>:-flto>
		)

		set(GOFLAGS
			${GOFLAGS}
			-a
			-ldflags '-w -extldflags \"-Wl,-undefined -Wl,dynamic_lookup\"'
		)

	else(APPLE)

		message("++ Setting Linux/BSD/Posix Compiler Flags (${CMAKE_BUILD_TYPE})")
		add_compile_options(
			-Wall
			-Wno-deprecated
			-Wno-unused-function
			-Wno-format
			$<$<CONFIG:DEBUG>:-g>
			$<$<CONFIG:DEBUG>:-O0>
			$<$<CONFIG:RELEASE>:-O3>
			$<$<CONFIG:RELEASE>:-ffast-math>
			$<$<CONFIG:RELEASE>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-O3>
			$<$<CONFIG:RELWITHDEBINFO>:-fPIE>
			$<$<CONFIG:RELWITHDEBINFO>:-g>
		)

		option(BUILD_32BIT "Force building as 32-bit binary" OFF)
		option(BUILD_STATIC "Build statically linked executable" OFF)

		if(BUILD_32BIT)
			set(CMAKE_SYSTEM_PROCESSOR "x86" CACHE STRING "system processor")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32" CACHE STRING "c++ flags")
			set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m32" CACHE STRING "c flags")
			set(GOARCH "GOARCH=386" CACHE STRING "go architecture")
			add_compile_options(
				-m32
			)
		endif(BUILD_32BIT)

		if(BUILD_STATIC)
			add_link_options(
				-static
			)
			set(CMAKE_EXE_LINKER_FLAGS "-static ${CMAKE_EXE_LINKER_FLAGS}")
			set(GOFLAGS 
				${GOFLAGS}
				-a 
				-tags netgo
				-ldflags '-w -extldflags \"-static -Wl,-unresolved-symbols=ignore-all\"'
			)
		else(BUILD_STATIC)
			set(GOFLAGS
				${GOFLAGS}
				-a
				-tags netgo
				-ldflags '-w -extldflags \"-Wl,-unresolved-symbols=ignore-all\"'
			)
		endif(BUILD_STATIC)

	endif(APPLE)
endif(WIN32)

if (
	CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "amd64" OR
	CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64"
)
	message("++ Adding flags for processor ${CMAKE_SYSTEM_PROCESSOR}")
	add_compile_options(-maes -mrdrnd -mpclmul -msse -msse2 -mssse3)
endif()

set(GO_BUILD_TAGS)

if(BUILD_CENTRAL_CONTROLLER)
	add_definitions(-DZT_CONTROLLER_USE_LIBPQ=1)
	set(GO_BUILD_TAGS -tags central)
endif(BUILD_CENTRAL_CONTROLLER)

add_subdirectory(core)
add_subdirectory(controller)
add_subdirectory(osdep)
add_subdirectory(serviceiocore)

file(GLOB go_src 
	${CMAKE_SOURCE_DIR}/cmd/*.go
	${CMAKE_SOURCE_DIR}/cmd/cmd/*.go
	${CMAKE_SOURCE_DIR}/pkg/zerotier/*.go)

if(WIN32)
	set(GO_EXE_NAME "zerotier.exe")
	set(GO_EXTRA_LIBRARIES "-lstdc++ -lwsock32 -lws2_32 -liphlpapi -lole32 -loleaut32 -lrpcrt4 -luuid")
else(WIN32)
	set(GO_EXE_NAME "zerotier")
	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		set(GO_EXTRA_LIBRARIES "-lstdc++")
	else()
		set(GO_EXTRA_LIBRARIES "-lc++")
	endif()
endif(WIN32)

add_custom_target(
	zerotier ALL
	BYPRODUCTS ${CMAKE_BINARY_DIR}/zerotier
	SOURCES ${go_src}
	COMMAND ${CMAKE_COMMAND} -E env ${GOARCH} CGO_ENABLED=1 CGO_CFLAGS=\"-O3\" CGO_LDFLAGS=\"$<TARGET_FILE:zt_core> $<TARGET_FILE:zt_controller> $<TARGET_FILE:zt_service_io_core> $<TARGET_FILE:zt_osdep> ${GO_EXTRA_LIBRARIES}\" ${GO} build ${GOFLAGS} -o ${CMAKE_BINARY_DIR}/${GO_EXE_NAME} ${CMAKE_SOURCE_DIR}/cmd/zerotier/zerotier.go
	COMMENT "Compiling Go Code..."
)
add_dependencies(zerotier zt_osdep zt_core zt_controller zt_service_io_core)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/zerotier DESTINATION bin)
if("${CMAKE_SYSTEM_NAME}" EQUAL "Linux")
	if(IS_DIRECTORY /lib/systemd/system)
		install(
			FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/zerotier.service 
			DESTINATION /lib/systemd/system
		)
	elseif(IS_DIRECTORY /usr/lib/systemd/system)
		install(
			FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/zerotier.service 
			DESTINATION /usr/lib/systemd/system
		)
	else()
		install(
			FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/zerotier.init
			DESTINATION /etc/init.d
		)
	endif()
endif()

if("${ZT_PACKAGE_FORMAT}" MATCHES "DEB")
	include(packaging/debian.cmake)
elseif("${ZT_PACKAGE_FORMAT}" MATCHES "RPM")
	include(packaging/rpm.cmake)
else()

endif()
