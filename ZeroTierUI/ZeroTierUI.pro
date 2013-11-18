#-------------------------------------------------
#
# Project created by QtCreator 2013-11-13T15:03:09
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ZeroTierUI
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    network.cpp \
    aboutwindow.cpp

HEADERS  += mainwindow.h \
    network.h \
    aboutwindow.h \
    ../node/Node.hpp

FORMS    += mainwindow.ui \
    network.ui \
    aboutwindow.ui

RESOURCES += \
    resources.qrc
