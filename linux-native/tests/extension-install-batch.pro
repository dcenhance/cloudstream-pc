QT += testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_extension_install_batch
SOURCES += test_extension_install_batch.cpp \
    ../extensions/ExtensionInstallBatch.cpp
HEADERS += ../extensions/ExtensionInstallBatch.h \
    ../extensions/ExtensionRegistry.h \
    ../repositories/RepositoryManifestParser.h
