#include "mpvrenderer.h"

#include <QObject>
#include <QtGlobal>
#include <QOpenGLContext>

#include <QtGui/QOpenGLFramebufferObject>

#include <QtQuick/QQuickWindow>
#include <qsgsimpletexturenode.h>

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
    static void *get_proc_address(void *ctx, const char *name) {
        (void)ctx;
        QOpenGLContext *glctx = QOpenGLContext::currentContext();
        if (!glctx)
            return NULL;
        return (void *)glctx->getProcAddress(QByteArray(name));
    }

    mpv_opengl_cb_context *mpv_gl;
    QQuickWindow *window;
public:
    MpvRenderer(mpv_opengl_cb_context *a_mpv_gl)
        : mpv_gl(a_mpv_gl), window(NULL)
    {
        int r = mpv_opengl_cb_init_gl(mpv_gl, NULL, get_proc_address, NULL);
        if (r < 0)
            throw "could not initialize OpenGL";
    }

    virtual ~MpvRenderer()
    {
        mpv_opengl_cb_uninit_gl(mpv_gl);
    }

    void render()
    {
        assert(window); // must have been set by synchronize()

        QOpenGLFramebufferObject *fbo = framebufferObject();
        int vp[4] = {0, 0, fbo->width(), fbo->height()};
        window->resetOpenGLState();
        mpv_opengl_cb_render(mpv_gl, fbo->handle(), vp);
        window->resetOpenGLState();
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size)
    {
        QOpenGLFramebufferObjectFormat format;
        return new QOpenGLFramebufferObject(size, format);
    }

protected:
    virtual void synchronize(QQuickFramebufferObject *item)
    {
        window = item->window();
    }
};

MpvObject::MpvObject(QQuickItem * parent)
    : QQuickFramebufferObject(parent)
{
    mpv = mpv_create();
    if (!mpv)
        throw "could not create mpv context";

    mpv_set_option_string(mpv, "terminal", "yes");
    mpv_set_option_string(mpv, "msg-level", "all=v");

    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        throw "could not initialize mpv context";
    }

    // Make use of the MPV_SUB_API_OPENGL_CB API.
    mpv::qt::set_option_variant(mpv, "vo", "opengl-cb");

    // Request hw decoding, just for testing.
    mpv::qt::set_option_variant(mpv, "hwdec", "auto");

    mpv_gl = (mpv_opengl_cb_context *)mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
    if (!mpv_gl) {
        mpv_terminate_destroy(mpv);
        throw "OpenGL not compiled in";
    }

    mpv_opengl_cb_set_update_callback(mpv_gl, on_update, (void *)this);

    connect(this, &MpvObject::onUpdate, this, &MpvObject::doUpdate, Qt::QueuedConnection);
}

MpvObject::~MpvObject()
{
    mpv_terminate_destroy(mpv);
}

void MpvObject::on_update(void *ctx)
{
    MpvObject *self = (MpvObject *)ctx;
    emit self->onUpdate();
}

// connected to onUpdate(); signal makes sure it runs on the GUI thread
void MpvObject::doUpdate()
{
    update();
}

void MpvObject::loadfile(const QString& filename)
{
    QVariantList cmd;
    cmd.append("loadfile");
    cmd.append(filename);
    mpv::qt::command_variant(mpv, cmd);
}

QQuickFramebufferObject::Renderer *MpvObject::createRenderer() const
{
    window()->setPersistentOpenGLContext(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(mpv_gl);
}
