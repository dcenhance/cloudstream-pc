QT += widgets testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_provider_picker_dialog
SOURCES += test_provider_picker_dialog.cpp \
    ../providers/ProviderPickerDialog.cpp \
    ../providers/ProviderSelectionModel.cpp \
    ../providers/ProviderPreferenceFilter.cpp \
    ../ui/SmoothScrollController.cpp
HEADERS += ../providers/ProviderPickerDialog.h \
    ../providers/ProviderSelectionModel.h \
    ../providers/ProviderPreferenceFilter.h \
    ../ui/SmoothScrollController.h
RESOURCES += ../resources.qrc
