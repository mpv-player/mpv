#ifndef MPVRENDERER_H_
#define MPVRENDERER_H_

#include <QtQuick/QQuickItem>

#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <mpv/qthelper.hpp>

class MpvRenderer : public QObject
{
    Q_OBJECT
    mpv::qt::Handle mpv;
    mpv_opengl_cb_context *mpv_gl;
    QQuickWindow *window;
    QSize size;

    friend class MpvObject;
public:
    MpvRenderer(mpv::qt::Handle a_mpv, mpv_opengl_cb_context *a_mpv_gl);
    virtual ~MpvRenderer();
public slots:
    void paint();
};

class MpvObject : public QQuickItem
{
    Q_OBJECT

    mpv::qt::Handle mpv;
    mpv_opengl_cb_context *mpv_gl;
    MpvRenderer *renderer;

public:
    MpvObject(QQuickItem * parent = 0);
    virtual ~MpvObject();
public slots:
    void command(const QVariant& params);
    void sync();
    void swapped();
    void cleanup();
signals:
    void onUpdate();
private slots:
    void doUpdate();
    void handleWindowChanged(QQuickWindow *win);
private:
    static void on_update(void *ctx);
};

#endif
