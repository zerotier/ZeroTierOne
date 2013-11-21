QT       += core gui widgets
TARGET = "ZeroTier One"
TEMPLATE = app

win32:RC_FILE = ZeroTierUI.rc
mac:ICON = zt1icon.icns
mac:QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
mac:QMAKE_INFO_PLIST = Info.plist

# ZeroTier One must be built before building this, since it links in the
# client code and some stuff from Utils to talk to the running service.
LIBS += ../node/*.o

SOURCES += main.cpp\
				mainwindow.cpp \
		network.cpp \
		aboutwindow.cpp

HEADERS  += mainwindow.h \
		network.h \
		aboutwindow.h \
		../node/Node.hpp \
		../node/Utils.hpp \
		../node/Defaults.hpp

FORMS    += mainwindow.ui \
		network.ui \
		aboutwindow.ui

RESOURCES += \
		resources.qrc
