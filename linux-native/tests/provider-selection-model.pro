QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_provider_selection_model
SOURCES += test_provider_selection_model.cpp \
    ../providers/ProviderSelectionModel.cpp \
    ../providers/ProviderPreferenceFilter.cpp
HEADERS += ../providers/ProviderSelectionModel.h \
    ../providers/ProviderPreferenceFilter.h
