cmake_minimum_required (VERSION 3.0)
project(zt_core)

configure_file(
	version.h.in
	version.h
)

set(core_headers
	zerotier.h
	Address.hpp
	Blob.hpp
	Buf.hpp
	C25519.hpp
	CapabilityCredential.hpp
	Certificate.hpp
	Defaults.hpp
	MembershipCredential.hpp
	OwnershipCredential.hpp
	Constants.hpp
	Containers.hpp
	Credential.hpp
	Defragmenter.hpp
	Dictionary.hpp
	ECC384.hpp
	EphemeralKey.hpp
	Expect.hpp
	FCV.hpp
	Fingerprint.hpp
	Identity.hpp
	InetAddress.hpp
	Locator.hpp
	LZ4.hpp
	MAC.hpp
	Member.hpp
	MIMC52.hpp
	MulticastGroup.hpp
	Mutex.hpp
	Network.hpp
	NetworkConfig.hpp
	Node.hpp
	OS.hpp
	Path.hpp
	Peer.hpp
	Poly1305.hpp
	Protocol.hpp
	RuntimeEnvironment.hpp
	Salsa20.hpp
	ScopedPtr.hpp
	SelfAwareness.hpp
	SHA512.hpp
	SharedPtr.hpp
	Spinlock.hpp
	SymmetricKey.hpp
	TagCredential.hpp
	Topology.hpp
	Trace.hpp
	TriviallyCopyable.hpp
	Utils.hpp
	VL1.hpp
	VL2.hpp
)

set(core_src
	AES.cpp
	AES_aesni.cpp
	AES_armcrypto.cpp
	Buf.cpp
	C25519.cpp
	CAPI.cpp
	CapabilityCredential.cpp
	Certificate.cpp
	Defaults.cpp
	MembershipCredential.cpp
	OwnershipCredential.cpp
	Credential.cpp
	Dictionary.cpp
	ECC384.cpp
	Endpoint.cpp
	Identity.cpp
	InetAddress.cpp
	Locator.cpp
	LZ4.cpp
	Member.cpp
	MIMC52.cpp
	Network.cpp
	NetworkConfig.cpp
	Node.cpp
	Path.cpp
	Peer.cpp
	Poly1305.cpp
	RevocationCredential.cpp
	Salsa20.cpp
	SelfAwareness.cpp
	SHA512.cpp
	TagCredential.cpp
	Topology.cpp
	Trace.cpp
	Utils.cpp
	VL1.cpp
	VL2.cpp
)

add_library(${PROJECT_NAME} STATIC ${core_src} ${core_headers})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

if(WIN32)
	set(libs ${libs} wsock32 ws2_32 rpcrt4 iphlpapi)
	target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
else(WIN32)
	set(libs ${libs} pthread)
	if (APPLE)
		target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
	else(APPLE)
		target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
	endif(APPLE)
endif(WIN32)

add_executable(zt_core_tests Tests.h Tests.cpp)
target_compile_definitions(zt_core_tests PRIVATE ZT_ENABLE_TESTS=1 ZT_STANDALONE_TESTS=1)
target_include_directories(
	${PROJECT_NAME}
	PUBLIC
	${CMAKE_BINARY_DIR}/core
)
target_link_libraries(zt_core_tests zt_core ${libs})
