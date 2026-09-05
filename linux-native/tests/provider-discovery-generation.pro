QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_provider_discovery_generation
SOURCES += test_provider_discovery_generation.cpp
HEADERS += ../providers/ProviderDiscoveryGeneration.h
