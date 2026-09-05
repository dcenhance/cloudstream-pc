QT += core testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_provider_configuration
SOURCES += test_provider_configuration.cpp \
           ../providers/ProviderConfiguration.cpp
HEADERS += ../providers/ProviderConfiguration.h
