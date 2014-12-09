#include <QGuiApplication>

#include <QtQuick/QQuickView>

#include "mpvrenderer.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<MpvObject>("mpvtest", 1, 0, "MpvObject");

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:///mpvtest/main.qml"));
    view.show();

    return app.exec();
}
