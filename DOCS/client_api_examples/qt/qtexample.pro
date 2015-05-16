QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qtexample
TEMPLATE = app

QT_CONFIG -= no-pkg-config
CONFIG += link_pkgconfig debug
PKGCONFIG += mpv

SOURCES += qtexample.cpp
HEADERS += qtexample.h
