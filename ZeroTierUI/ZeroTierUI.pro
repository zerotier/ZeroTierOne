QT       += core gui widgets network
TARGET = "ZeroTier One"
TEMPLATE = app

win32:RC_FILE = ZeroTierUI.rc
win32:LIBS += winhttp.lib Iphlpapi.lib ws2_32.lib advapi32.lib Shell32.lib Rpcrt4.lib
win32:QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"

mac:ICON = zt1icon.icns
mac:QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
mac:QMAKE_INFO_PLIST = Info.plist
mac:LIBS += -framework Cocoa

SOURCES += main.cpp \
		mainwindow.cpp \
		aboutwindow.cpp \
		networkwidget.cpp \
		installdialog.cpp \
		licensedialog.cpp \
		onetimedialog.cpp \
    ../node/C25519.cpp \
    ../node/CertificateOfMembership.cpp \
    ../node/Defaults.cpp \
    ../node/HttpClient.cpp \
    ../node/Identity.cpp \
    ../node/InetAddress.cpp \
    ../node/IpcConnection.cpp \
    ../node/IpcListener.cpp \
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
    ../node/SocketManager.cpp \
    ../node/SoftwareUpdater.cpp \
    ../node/Switch.cpp \
    ../node/SysEnv.cpp \
    ../node/TcpSocket.cpp \
    ../node/Topology.cpp \
    ../node/UdpSocket.cpp \
    ../node/Utils.cpp \
    ../ext/lz4/lz4.c \
    ../ext/lz4/lz4hc.c

HEADERS  += mainwindow.h \
		aboutwindow.h \
		networkwidget.h \
		installdialog.h \
		mac_doprivileged.h \
		licensedialog.h \
		main.h \
		onetimedialog.h \
    ../node/Address.hpp \
    ../node/Array.hpp \
    ../node/AtomicCounter.hpp \
    ../node/BandwidthAccount.hpp \
    ../node/Buffer.hpp \
    ../node/C25519.hpp \
    ../node/CertificateOfMembership.hpp \
    ../node/CMWC4096.hpp \
    ../node/Constants.hpp \
    ../node/Defaults.hpp \
    ../node/Dictionary.hpp \
    ../node/HttpClient.hpp \
    ../node/Identity.hpp \
    ../node/InetAddress.hpp \
    ../node/IpcConnection.hpp \
    ../node/IpcListener.hpp \
    ../node/Logger.hpp \
    ../node/MAC.hpp \
    ../node/Multicaster.hpp \
    ../node/MulticastGroup.hpp \
    ../node/Mutex.hpp \
    ../node/Network.hpp \
    ../node/NetworkConfig.hpp \
    ../node/Node.hpp \
    ../node/NodeConfig.hpp \
    ../node/NonCopyable.hpp \
    ../node/Packet.hpp \
    ../node/PacketDecoder.hpp \
    ../node/Path.hpp \
    ../node/Peer.hpp \
    ../node/Poly1305.hpp \
    ../node/RuntimeEnvironment.hpp \
    ../node/Salsa20.hpp \
    ../node/Service.hpp \
    ../node/SHA512.hpp \
    ../node/SharedPtr.hpp \
    ../node/Socket.hpp \
    ../node/SocketManager.hpp \
    ../node/SoftwareUpdater.hpp \
    ../node/Switch.hpp \
    ../node/SysEnv.hpp \
    ../node/TcpSocket.hpp \
    ../node/Thread.hpp \
    ../node/Topology.hpp \
    ../node/UdpSocket.hpp \
    ../node/Utils.hpp \
    ../ext/lz4/lz4.h \
    ../ext/lz4/lz4hc.h

win32:SOURCES += ../node/WindowsEthernetTap.cpp
mac:SOURCE += ../node/UnixEthernetTap.cpp

FORMS    += mainwindow.ui \
		aboutwindow.ui \
		networkwidget.ui \
		installdialog.ui \
		licensedialog.ui \
		quickstartdialog.ui \
		onetimedialog.ui

RESOURCES += \
		resources.qrc

mac:OBJECTIVE_SOURCES += \
		mac_doprivileged.mm

OTHER_FILES += \
		stylesheet.css
