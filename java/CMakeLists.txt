cmake_minimum_required(VERSION 3.2)

project(ZeroTierOneJNI)

find_package(Java COMPONENTS Development)
message("JAVA_HOME: $ENV{JAVA_HOME}")

set(Java_INCLUDE_DIRS $ENV{JAVA_HOME}/include)

message("Java Include Dirs: ${Java_INCLUDE_DIRS}")

if(WIN32)
    add_definitions(-DNOMINMAX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W3 /MP")
endif()

set(src_files
    ../ext/lz4/lz4.c
    ../ext/json-parser/json.c
    ../ext/http-parser/http_parser.c
    ../node/C25519.cpp
    ../node/CertificateOfMembership.cpp
    ../node/Defaults.cpp
    ../node/Dictionary.cpp
    ../node/Identity.cpp
    ../node/IncomingPacket.cpp
    ../node/InetAddress.cpp
    ../node/Multicaster.cpp
    ../node/Network.cpp
    ../node/NetworkConfig.cpp
    ../node/Node.cpp
    ../node/OutboundMulticast.cpp
    ../node/Packet.cpp
    ../node/Peer.cpp
    ../node/Poly1305.cpp
    ../node/Salsa20.cpp
    ../node/SelfAwareness.cpp
    ../node/SHA512.cpp
    ../node/Switch.cpp
    ../node/Topology.cpp
    ../node/Utils.cpp
    ../osdep/Http.cpp
    ../osdep/OSUtils.cpp
    jni/com_zerotierone_sdk_Node.cpp
    jni/ZT1_jniutils.cpp
    jni/ZT1_jnicache.cpp
    )

set(include_dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/
    ${Java_INCLUDE_DIRS})

if(WIN32)
    set(include_dirs
        ${include_dirs}
        ${Java_INCLUDE_DIRS}/win32)
endif()

include_directories(
    ${include_dirs}
    )

add_library(${PROJECT_NAME} SHARED ${src_files})

set(link_libs )

if(WIN32)
    set(link_libs
        wsock32
        ws2_32

        )
endif()

target_link_libraries(${PROJECT_NAME} ${link_libs})