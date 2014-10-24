#-------------------------------------------------
#
# Project created by QtCreator 2014-10-24T10:37:18
#
#-------------------------------------------------

QT       += core network

QT       -= gui

QMAKE_CXXFLAGS += -std=c++11

include($$PWD/smppcxx/smppcxx.pri)

TARGET = smpp-server
#CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    smppserver.cpp \
    smppsession.cpp

HEADERS += \
    smppserver.h \
    smppsession.h
