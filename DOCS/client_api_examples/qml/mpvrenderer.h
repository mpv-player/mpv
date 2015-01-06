#ifndef MPVRENDERER_H_
#define MPVRENDERER_H_

#include <QtQuick/QQuickFramebufferObject>

#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <mpv/qthelper.hpp>

class MpvRenderer;

class MpvObject : public QQuickFramebufferObject
{
    Q_OBJECT

    mpv::qt::Handle mpv;
    mpv_opengl_cb_context *mpv_gl;

    friend class MpvRenderer;

public:
    MpvObject(QQuickItem * parent = 0);
    virtual ~MpvObject();
    virtual Renderer *createRenderer() const;
public slots:
    void command(const QVariant& params);
    void setProperty(const QString& name, const QVariant& value);
signals:
    void onUpdate();
private slots:
    void doUpdate();
private:
    static void on_update(void *ctx);
};

#endif
