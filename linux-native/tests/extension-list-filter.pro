QT += core testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_extension_list_filter
SOURCES += test_extension_list_filter.cpp \
           ../extensions/ExtensionListFilter.cpp
HEADERS += ../extensions/ExtensionListFilter.h
