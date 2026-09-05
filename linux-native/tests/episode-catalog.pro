QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_episode_catalog
SOURCES += test_episode_catalog.cpp \
    ../episodes/EpisodeCatalog.cpp
HEADERS += ../episodes/EpisodeCatalog.h
