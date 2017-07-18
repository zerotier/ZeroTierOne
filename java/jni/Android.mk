LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ZeroTierOneJNI
LOCAL_C_INCLUDES := \
    $(ZT1)/include \
    $(ZT1)/node \
    $(ZT1)/osdep \
    $(ZT1)/ext/miniupnpc \
    $(ZT1)/ext/libnatpmp

LOCAL_LDLIBS := -llog -latomic
# LOCAL_CFLAGS := -g

LOCAL_CFLAGS := -DZT_USE_MINIUPNPC

# ZeroTierOne SDK source files
LOCAL_SRC_FILES := \
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
	$(ZT1)/osdep/OSUtils.cpp \
	$(ZT1)/osdep/PortMapper.cpp

# libminiupnpc and libnatpmp files
LOCAL_SRC_FILES += \
	$(ZT1)/ext/miniupnpc/connecthostport.c \
	$(ZT1)/ext/miniupnpc/igd_desc_parse.c \
	$(ZT1)/ext/miniupnpc/minisoap.c \
	$(ZT1)/ext/miniupnpc/minissdpc.c \
	$(ZT1)/ext/miniupnpc/miniupnpc.c \
	$(ZT1)/ext/miniupnpc/miniwget.c \
	$(ZT1)/ext/miniupnpc/minixml.c \
	$(ZT1)/ext/miniupnpc/portlistingparse.c \
	$(ZT1)/ext/miniupnpc/receivedata.c \
	$(ZT1)/ext/miniupnpc/upnpcommands.c \
	$(ZT1)/ext/miniupnpc/upnpdev.c \
	$(ZT1)/ext/miniupnpc/upnperrors.c \
	$(ZT1)/ext/miniupnpc/upnpreplyparse.c \
	$(ZT1)/ext/libnatpmp/natpmp.c \
	$(ZT1)/ext/libnatpmp/getgateway.c

# JNI Files
LOCAL_SRC_FILES += \
	com_zerotierone_sdk_Node.cpp \
	ZT_jniutils.cpp \
	ZT_jnilookup.cpp

include $(BUILD_SHARED_LIBRARY)