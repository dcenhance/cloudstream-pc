QT += widgets testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_settings_pane
SOURCES += test_settings_pane.cpp \
    ../settings/SettingsPane.cpp \
    ../storage/XdgPaths.cpp \
    ../ui/SmoothScrollController.cpp
HEADERS += ../settings/SettingsPane.h \
    ../storage/XdgPaths.h \
    ../ui/SmoothScrollController.h
RESOURCES += ../resources.qrc
