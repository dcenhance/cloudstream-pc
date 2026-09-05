QT += testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_provider_preference_filter
SOURCES += test_provider_preference_filter.cpp \
    ../providers/ProviderPreferenceFilter.cpp
HEADERS += ../providers/ProviderPreferenceFilter.h
