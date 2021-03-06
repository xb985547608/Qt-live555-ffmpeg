#-------------------------------------------------
#
# Project created by QtCreator 2018-04-11T20:36:50
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = live555-test
TEMPLATE = app

MOC_DIR = $$PWD/moc
OBJECTS_DIR = $$PWD/obj
RCC_DIR = $$PWD/rcc

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
    DynamicRTSPServer.cpp

HEADERS += \
    DynamicRTSPServer.hh \
    version.hh

include($$PWD/live555-latest-0.91/live555.pri)
