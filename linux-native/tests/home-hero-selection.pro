QT += core testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_home_hero_selection
SOURCES += test_home_hero_selection.cpp \
           ../providers/HomeHeroSelection.cpp
HEADERS += ../providers/HomeHeroSelection.h
