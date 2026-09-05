include(../cloudstream-linux.pro)
QT += testlib
TARGET = test_single_window_surfaces
SOURCES -= main.cpp
for(source, SOURCES): TEST_SOURCES += $$absolute_path($$source, $$PWD/..)
SOURCES = $$TEST_SOURCES $$PWD/test_single_window_surfaces.cpp
for(header, HEADERS): TEST_HEADERS += $$absolute_path($$header, $$PWD/..)
HEADERS = $$TEST_HEADERS
RESOURCES = $$PWD/../resources.qrc
