QT += testlib core
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_repository_url_resolver
SOURCES += test_repository_url_resolver.cpp \
    ../repositories/RepositoryUrlResolver.cpp
HEADERS += ../repositories/RepositoryUrlResolver.h
