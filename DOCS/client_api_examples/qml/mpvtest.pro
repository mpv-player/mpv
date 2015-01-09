QT += qml quick

HEADERS += main.h
SOURCES += main.cpp

CONFIG += link_pkgconfig debug
PKGCONFIG += mpv

RESOURCES += mpvtest.qrc

OTHER_FILES += main.qml
