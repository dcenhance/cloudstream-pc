QT += core testlib
CONFIG += console c++17 testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_details_presentation
SOURCES += test_details_presentation.cpp \
           ../details/DetailsPresentation.cpp
HEADERS += ../details/DetailsPresentation.h
