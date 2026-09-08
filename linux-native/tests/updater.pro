QT += widgets network testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_updater
include(../updates/updates.pri)
SOURCES += test_updater.cpp
RESOURCES += ../resources.qrc
