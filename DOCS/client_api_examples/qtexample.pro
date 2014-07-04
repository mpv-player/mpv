QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qtexample
TEMPLATE = app

CONFIG += link_pkgconfig
PKGCONFIG = mpv

SOURCES += qtexample.cpp
HEADERS += qtexample.h
