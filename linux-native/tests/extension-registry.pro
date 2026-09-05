QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_extension_registry
SOURCES += test_extension_registry.cpp \
    ../extensions/ExtensionRegistry.cpp
HEADERS += ../extensions/ExtensionRegistry.h
