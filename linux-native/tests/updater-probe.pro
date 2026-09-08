QT += core network
QT -= gui
CONFIG += console c++17
TEMPLATE = app
TARGET = updater_probe
CS_VERSION = $$cat($$PWD/../VERSION, lines)
DEFINES += CLOUDSTREAM_VERSION=\\\"$$CS_VERSION\\\"
SOURCES += updater_probe.cpp ../updates/ReleaseUpdater.cpp ../network/CloudStreamRequest.cpp
HEADERS += ../updates/ReleaseUpdater.h
