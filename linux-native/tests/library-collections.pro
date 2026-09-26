QT += testlib core
CONFIG += testcase c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_library_collection_store
SOURCES += test_library_collection_store.cpp \
    ../history/LibraryCollectionStore.cpp \
    ../history/WatchHistoryStore.cpp
HEADERS += ../history/LibraryCollectionStore.h \
    ../history/WatchHistoryStore.h
