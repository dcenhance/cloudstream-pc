QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_provider_validation
SOURCES += test_provider_validation.cpp \
    ../providers/ProviderValidation.cpp
HEADERS += ../providers/ProviderValidation.h
