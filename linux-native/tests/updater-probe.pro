QT += core network
QT -= gui
CONFIG += console c++17
TEMPLATE = app
TARGET = updater_probe
CS_VERSION = $$cat($$PWD/../VERSION.txt, lines)
isEmpty(CS_VERSION): error(Missing CloudStream VERSION.txt)
DEFINES += CLOUDSTREAM_VERSION=\\\"$$CS_VERSION\\\"
SOURCES += updater_probe.cpp ../updates/ReleaseUpdater.cpp ../network/CloudStreamRequest.cpp
HEADERS += ../updates/ReleaseUpdater.h
