include(../cloudstream-linux.pro)
QT += testlib
TARGET = test_home_render_yield
SOURCES -= main.cpp
for(source, SOURCES): TEST_SOURCES += $$absolute_path($$source, $$PWD/..)
SOURCES = $$TEST_SOURCES $$PWD/test_home_render_yield.cpp
for(header, HEADERS): TEST_HEADERS += $$absolute_path($$header, $$PWD/..)
HEADERS = $$TEST_HEADERS
RESOURCES = $$PWD/../resources.qrc
