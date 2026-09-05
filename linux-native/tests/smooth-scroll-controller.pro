QT += widgets testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_smooth_scroll_controller
SOURCES += test_smooth_scroll_controller.cpp \
    ../ui/SmoothScrollController.cpp
HEADERS += ../ui/SmoothScrollController.h
