#-------------------------------------------------
#
# Project created by QtCreator 2013-11-13T15:03:09
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ZeroTierUI
TEMPLATE = app

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
