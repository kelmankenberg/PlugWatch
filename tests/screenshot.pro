QT += core gui qml quick quickcontrols2 network
CONFIG += c++17 release
TARGET = plugwatch_screenshot
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += screenshot.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc
