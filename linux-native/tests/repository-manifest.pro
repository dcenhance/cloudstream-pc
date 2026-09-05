QT += testlib core
CONFIG += console testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_repository_manifest_parser
SOURCES += test_repository_manifest_parser.cpp \
    ../repositories/RepositoryManifestParser.cpp
HEADERS += ../repositories/RepositoryManifestParser.h
