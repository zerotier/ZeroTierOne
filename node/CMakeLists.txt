cmake_minimum_required (VERSION 2.8)
project(zt_core)

if(WIN32)
	add_definitions(-DNOMINMAX)
endif(WIN32)

set(core_headers
	Address.hpp
	AtomicCounter.hpp
	Buffer.hpp
	C25519.hpp
	Capability.hpp
	CertificateOfMembership.hpp
	CertificateOfOwnership.hpp
	Constants.hpp
	Credential.hpp
	Dictionary.hpp
	ECC384.hpp
	EphemeralKey.hpp
	Hashtable.hpp
	Identity.hpp
	InetAddress.hpp
	Locator.hpp
	MAC.hpp
	Membership.hpp
	MulticastGroup.hpp
	Mutex.hpp
	Network.hpp
	NetworkConfig.hpp
	Node.hpp
	Packet.hpp
	Path.hpp
	Peer.hpp
	Poly1305.hpp
	RingBuffer.hpp
	RuntimeEnvironment.hpp
	Salsa20.hpp
	ScopedPtr.hpp
	SelfAwareness.hpp
	SHA512.hpp
	SharedPtr.hpp
	Str.hpp
	Switch.hpp
	Tag.hpp
	Topology.hpp
	Trace.hpp
	Utils.hpp
)

set(core_src
	C25519.cpp
	Credential.cpp
	ECC384.cpp
	Identity.cpp
	IncomingPacket.cpp
	InetAddress.cpp
	Membership.cpp
	Network.cpp
	NetworkConfig.cpp
	Node.cpp
	Packet.cpp
	Path.cpp
	Peer.cpp
	Poly1305.cpp
	Salsa20.cpp
	SelfAwareness.cpp
	SHA512.cpp
	Switch.cpp
	Trace.cpp
	Utils.cpp
)

add_library(${PROJECT_NAME} STATIC ${core_src} ${core_headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

#if(UNIX)
#	set_source_files_properties(
#		AES.cpp
#		AES-aesni.c
#		ECC384.cpp
#		Salsa20.cpp
#		C25519.cpp
#		Poly1305.cpp
#		PROPERTIES
#		COMPILE_FLAGS "-Wall -O3"
#	)
#endif(UNIX)
