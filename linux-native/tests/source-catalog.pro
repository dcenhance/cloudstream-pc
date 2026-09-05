QT += testlib core network
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_source_catalog
SOURCES += test_source_catalog.cpp \
    ../player/SourceCatalog.cpp
HEADERS += ../player/SourceCatalog.h
