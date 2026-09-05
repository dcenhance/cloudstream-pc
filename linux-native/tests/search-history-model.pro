QT += core testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_search_history_model
SOURCES += test_search_history_model.cpp \
           ../search/SearchHistoryModel.cpp
HEADERS += ../search/SearchHistoryModel.h
