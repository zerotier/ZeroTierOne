QT       += core gui widgets
TARGET = "ZeroTier One"
TEMPLATE = app

win32:RC_FILE = ZeroTierUI.rc
mac:ICON = zt1icon.icns
mac:QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
mac:QMAKE_INFO_PLIST = Info.plist

SOURCES += main.cpp\
				mainwindow.cpp \
		aboutwindow.cpp \
		../node/C25519.cpp \
		../node/CertificateOfMembership.cpp \
		../node/Defaults.cpp \
		../node/Demarc.cpp \
		../node/EthernetTap.cpp \
		../node/HttpClient.cpp \
		../node/Identity.cpp \
		../node/InetAddress.cpp \
		../node/Logger.cpp \
		../node/Multicaster.cpp \
		../node/Network.cpp \
		../node/NetworkConfig.cpp \
		../node/Node.cpp \
		../node/NodeConfig.cpp \
		../node/Packet.cpp \
		../node/PacketDecoder.cpp \
		../node/Peer.cpp \
		../node/Poly1305.cpp \
		../node/Salsa20.cpp \
		../node/Service.cpp \
		../node/SHA512.cpp \
		../node/SoftwareUpdater.cpp \
		../node/Switch.cpp \
		../node/SysEnv.cpp \
		../node/Topology.cpp \
		../node/UdpSocket.cpp \
		../node/Utils.cpp \
    ../ext/lz4/lz4.c \
    ../ext/lz4/lz4hc.c \
    networkwidget.cpp

HEADERS  += mainwindow.h \
		aboutwindow.h \
		../node/Node.hpp \
		../node/Utils.hpp \
		../node/Defaults.hpp \
		../node/Address.hpp \
		../node/Array.hpp \
		../node/AtomicCounter.hpp \
		../node/BandwidthAccount.hpp \
		../node/Buffer.hpp \
		../node/C25519.hpp \
		../node/CertificateOfMembership.hpp \
		../node/CMWC4096.hpp \
		../node/Condition.hpp \
		../node/Constants.hpp \
		../node/Demarc.hpp \
		../node/Dictionary.hpp \
		../node/EthernetTap.hpp \
		../node/HttpClient.hpp \
		../node/Identity.hpp \
		../node/InetAddress.hpp \
		../node/Logger.hpp \
		../node/MAC.hpp \
		../node/Multicaster.hpp \
		../node/MulticastGroup.hpp \
		../node/Mutex.hpp \
		../node/Network.hpp \
		../node/NetworkConfig.hpp \
		../node/NodeConfig.hpp \
		../node/NonCopyable.hpp \
		../node/Packet.hpp \
		../node/PacketDecoder.hpp \
		../node/Peer.hpp \
		../node/Poly1305.hpp \
		../node/RuntimeEnvironment.hpp \
		../node/Salsa20.hpp \
		../node/Service.hpp \
		../node/SHA512.hpp \
		../node/SharedPtr.hpp \
		../node/SoftwareUpdater.hpp \
		../node/Switch.hpp \
		../node/SysEnv.hpp \
		../node/Thread.hpp \
		../node/Topology.hpp \
		../node/UdpSocket.hpp \
    ../ext/lz4/lz4.h \
    ../ext/lz4/lz4hc.h \
    networkwidget.h

FORMS    += mainwindow.ui \
		aboutwindow.ui \
    networkwidget.ui

RESOURCES += \
		resources.qrc
