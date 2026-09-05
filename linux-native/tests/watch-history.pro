QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_watch_history_store
SOURCES += test_watch_history_store.cpp \
    ../history/WatchHistoryStore.cpp
HEADERS += ../history/WatchHistoryStore.h
