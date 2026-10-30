QT += core gui network testlib
CONFIG += c++17 testcase
TARGET = plugwatch_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h plugwatch_tests.h
SOURCES += plugwatch_tests.cpp ../src/backend.cpp
