QT += testlib core gui network concurrent
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_artwork_loader
SOURCES += test_artwork_loader.cpp \
    ../media/ArtworkLoader.cpp \
    ../media/ArtworkSizing.cpp \
    ../network/CloudStreamRequest.cpp
HEADERS += ../media/ArtworkLoader.h \
    ../media/ArtworkSizing.h \
    ../network/CloudStreamRequest.h
