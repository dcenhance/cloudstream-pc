QT += network testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = test_network_request_policy
SOURCES += test_network_request_policy.cpp \
    ../network/CloudStreamRequest.cpp
HEADERS += ../network/CloudStreamRequest.h
