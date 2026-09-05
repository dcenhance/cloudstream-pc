QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_home_content_limiter
SOURCES += test_home_content_limiter.cpp \
    ../providers/HomeContentLimiter.cpp
HEADERS += ../providers/HomeContentLimiter.h
