QT += testlib network
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_download_manager
SOURCES += test_download_manager.cpp \
    ../downloads/DownloadManager.cpp \
    ../downloads/DownloadQueueStore.cpp \
    ../player/SourceCatalog.cpp
HEADERS += ../downloads/DownloadManager.h \
    ../downloads/DownloadQueueStore.h \
    ../player/SourceCatalog.h
