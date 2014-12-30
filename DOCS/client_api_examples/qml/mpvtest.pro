QT += qml quick

HEADERS += mpvrenderer.h
SOURCES += mpvrenderer.cpp main.cpp

CONFIG += link_pkgconfig debug
PKGCONFIG += mpv

RESOURCES += mpvtest.qrc

OTHER_FILES += main.qml
