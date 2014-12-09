#ifndef MPVRENDERER_H_
#define MPVRENDERER_H_

#include <assert.h>

#include <QtQuick/QQuickFramebufferObject>

#include "libmpv/client.h"
#include "libmpv/opengl_cb.h"
#include "libmpv/qthelper.hpp"

class MpvObject : public QQuickFramebufferObject
{
    Q_OBJECT

    mpv_handle *mpv;
    mpv_opengl_cb_context *mpv_gl;

public:
    MpvObject(QQuickItem * parent = 0);
    virtual ~MpvObject();
    Renderer *createRenderer() const;
public slots:
    void loadfile(const QString& filename);
signals:
    void onUpdate();
private slots:
    void doUpdate();
private:
    static void on_update(void *ctx);
};

#endif
