QT += testlib core
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_player_command
SOURCES += test_player_command.cpp \
    ../player/PlayerCommand.cpp
HEADERS += ../player/PlayerCommand.h
