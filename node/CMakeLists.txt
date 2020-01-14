cmake_minimum_required (VERSION 2.8)
project(zt_core)

set(core_headers
	Address.hpp
	AtomicCounter.hpp
	Buf.hpp
	Buffer.hpp
	C25519.hpp
	Capability.hpp
	CertificateOfMembership.hpp
	CertificateOfOwnership.hpp
	Constants.hpp
	Credential.hpp
	Dictionary.hpp
	ECC384.hpp
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
	OS.hpp
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
	AES.cpp
	Buf.cpp
	C25519.cpp
	Credential.cpp
	ECC384.cpp
	Endpoint.cpp
	Identity.cpp
	IncomingPacket.cpp
	InetAddress.cpp
	Locator.cpp
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
	Topology.cpp
	Utils.cpp
)

add_library(${PROJECT_NAME} STATIC ${core_src} ${core_headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})
