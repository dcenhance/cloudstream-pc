QT += testlib core gui
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_artwork_sizing
SOURCES += test_artwork_sizing.cpp \
    ../media/ArtworkSizing.cpp
HEADERS += ../media/ArtworkSizing.h
