include(../cloudstream-linux.pro)
QT += testlib
TARGET = test_search_pagination_gui
SOURCES -= main.cpp
for(source, SOURCES): TEST_SOURCES += $$absolute_path($$source, $$PWD/..)
SOURCES = $$TEST_SOURCES $$PWD/test_search_pagination_gui.cpp
for(header, HEADERS): TEST_HEADERS += $$absolute_path($$header, $$PWD/..)
HEADERS = $$TEST_HEADERS $$PWD/SearchPaginationGuiTest.h
RESOURCES = $$PWD/../resources.qrc
