QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_mpv_ipc_protocol
SOURCES += test_mpv_ipc_protocol.cpp \
    ../player/MpvIpcProtocol.cpp
HEADERS += ../player/MpvIpcProtocol.h
