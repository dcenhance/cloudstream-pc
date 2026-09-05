QT += widgets testlib
CONFIG += testcase c++17 console link_pkgconfig
CONFIG -= app_bundle
PKGCONFIG += sdl2
TEMPLATE = app
TARGET = test_gamepad_navigation
SOURCES += test_gamepad_navigation.cpp \
    ../input/GamepadNavigation.cpp
HEADERS += ../input/GamepadNavigation.h
