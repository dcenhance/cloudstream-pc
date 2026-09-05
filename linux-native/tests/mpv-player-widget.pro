QT += testlib widgets openglwidgets
CONFIG += testcase c++17 console link_pkgconfig
CONFIG -= app_bundle
TEMPLATE = app
TARGET = test_mpv_player_widget
SOURCES += test_mpv_player_widget.cpp \
    ../player/IntegratedPlayerWindow.cpp \
    ../player/MpvPlayerWidget.cpp \
    ../player/SourceCatalog.cpp
HEADERS += ../player/IntegratedPlayerWindow.h \
    ../player/MpvPlayerWidget.h \
    ../player/SourceCatalog.h
RESOURCES += ../resources.qrc
PKGCONFIG += mpv
win32 {
    CONFIG -= link_pkgconfig
    INCLUDEPATH += $$(MPV_INCLUDE)
    LIBS += $$(MPV_LIB)
}
