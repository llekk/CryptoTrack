QT += core gui widgets network

CONFIG += c++17

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CryptoTrack
TEMPLATE = app

SOURCES += \
    main.cpp \
    ai_project.cpp

HEADERS += \
    ai_project.h

FORMS += \
    ai_project.ui