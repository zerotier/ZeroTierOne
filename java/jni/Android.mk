LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ZeroTierOneJNI
LOCAL_C_INCLUDES := \
    $(ZT1)/include \
    $(ZT1)/node \
    $(ZT1)/osdep

LOCAL_LDLIBS := -llog
# LOCAL_CFLAGS := -g

LOCAL_CFLAGS := -DZT_USE_MINIUPNPC
ifeq ($(TARGET_ARCH_ABI),x86_64)
    LOCAL_CXXFLAGS := -maes -mpclmul -msse3 -msse4.1
endif
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_ARM_NEON := true
    LOCAL_CXXFLAGS := -march=armv8-a+crypto -mfloat-abi=softfp -mfpu=neon -maes
endif

# ZeroTierOne SDK source files
LOCAL_SRC_FILES := \
    $(ZT1)/node/AES.cpp \
    $(ZT1)/node/AES_aesni.cpp \
    $(ZT1)/node/AES_armcrypto.cpp \
    $(ZT1)/node/Bond.cpp \
    $(ZT1)/node/C25519.cpp \
	$(ZT1)/node/Capability.cpp \
	$(ZT1)/node/CertificateOfMembership.cpp \
	$(ZT1)/node/CertificateOfOwnership.cpp \
	$(ZT1)/node/Identity.cpp \
	$(ZT1)/node/IncomingPacket.cpp \
	$(ZT1)/node/InetAddress.cpp \
	$(ZT1)/node/Membership.cpp \
	$(ZT1)/node/Multicaster.cpp \
	$(ZT1)/node/Network.cpp \
	$(ZT1)/node/NetworkConfig.cpp \
	$(ZT1)/node/Node.cpp \
	$(ZT1)/node/OutboundMulticast.cpp \
	$(ZT1)/node/Packet.cpp \
	$(ZT1)/node/Path.cpp \
	$(ZT1)/node/Peer.cpp \
	$(ZT1)/node/Poly1305.cpp \
	$(ZT1)/node/Revocation.cpp \
	$(ZT1)/node/Salsa20.cpp \
	$(ZT1)/node/SelfAwareness.cpp \
	$(ZT1)/node/SHA512.cpp \
	$(ZT1)/node/Switch.cpp \
	$(ZT1)/node/Tag.cpp \
	$(ZT1)/node/Topology.cpp \
	$(ZT1)/node/Trace.cpp \
	$(ZT1)/node/Utils.cpp \
	$(ZT1)/osdep/OSUtils.cpp

# JNI Files
LOCAL_SRC_FILES += \
	com_zerotierone_sdk_Node.cpp \
	ZT_jniarray.cpp \
	ZT_jniutils.cpp \
	ZT_jnilookup.cpp

include $(BUILD_SHARED_LIBRARY)