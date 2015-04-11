#include "main.h"

#include <stdexcept>
#include <clocale>

#include <QObject>
#include <QtGlobal>
#include <QOpenGLContext>

#include <QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickView>

static void *get_proc_address(void *ctx, const char *name) {
    (void)ctx;
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx)
        return NULL;
    return (void *)glctx->getProcAddress(QByteArray(name));
}

MpvRenderer::MpvRenderer(mpv::qt::Handle a_mpv, mpv_opengl_cb_context *a_mpv_gl)
    : mpv(a_mpv), mpv_gl(a_mpv_gl), window(0), size()
{
    int r = mpv_opengl_cb_init_gl(mpv_gl, NULL, get_proc_address, NULL);
    if (r < 0)
        throw std::runtime_error("could not initialize OpenGL");
}

MpvRenderer::~MpvRenderer()
{
    // Until this call is done, we need to make sure the player remains
    // alive. This is done implicitly with the mpv::qt::Handle instance
    // in this class.
    mpv_opengl_cb_uninit_gl(mpv_gl);
}

void MpvRenderer::paint()
{
    window->resetOpenGLState();

    // This uses 0 as framebuffer, which indicates that mpv will render directly
    // to the frontbuffer. Note that mpv will always switch framebuffers
    // explicitly. Some QWindow setups (such as using QQuickWidget) actually
    // want you to render into a FBO in the beforeRendering() signal, and this
    // code won't work there.
    // The negation is used for rendering with OpenGL's flipped coordinates.
    mpv_opengl_cb_draw(mpv_gl, 0, size.width(), -size.height());

    window->resetOpenGLState();
}

MpvObject::MpvObject(QQuickItem * parent)
    : QQuickItem(parent), mpv_gl(0), renderer(0)
{
    mpv = mpv::qt::Handle::FromRawHandle(mpv_create());
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    mpv_set_option_string(mpv, "terminal", "yes");
    mpv_set_option_string(mpv, "msg-level", "all=v");

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");

    // Make use of the MPV_SUB_API_OPENGL_CB API.
    mpv::qt::set_option_variant(mpv, "vo", "opengl-cb");

    // Setup the callback that will make QtQuick update and redraw if there
    // is a new video frame. Use a queued connection: this makes sure the
    // doUpdate() function is run on the GUI thread.
    mpv_gl = (mpv_opengl_cb_context *)mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
    if (!mpv_gl)
        throw std::runtime_error("OpenGL not compiled in");
    mpv_opengl_cb_set_update_callback(mpv_gl, MpvObject::on_update, (void *)this);
    connect(this, &MpvObject::onUpdate, this, &MpvObject::doUpdate,
            Qt::QueuedConnection);

    connect(this, &QQuickItem::windowChanged,
            this, &MpvObject::handleWindowChanged);
}

MpvObject::~MpvObject()
{
    if (mpv_gl)
        mpv_opengl_cb_set_update_callback(mpv_gl, NULL, NULL);
}

void MpvObject::handleWindowChanged(QQuickWindow *win)
{
    if (!win)
        return;
    connect(win, &QQuickWindow::beforeSynchronizing,
            this, &MpvObject::sync, Qt::DirectConnection);
    connect(win, &QQuickWindow::sceneGraphInvalidated,
            this, &MpvObject::cleanup, Qt::DirectConnection);
    connect(win, &QQuickWindow::frameSwapped,
            this, &MpvObject::swapped, Qt::DirectConnection);
    win->setClearBeforeRendering(false);
}

void MpvObject::sync()
{
    if (!renderer) {
        renderer = new MpvRenderer(mpv, mpv_gl);
        connect(window(), &QQuickWindow::beforeRendering,
                renderer, &MpvRenderer::paint, Qt::DirectConnection);
    }
    renderer->window = window();
    renderer->size = window()->size() * window()->devicePixelRatio();
}

void MpvObject::swapped()
{
    mpv_opengl_cb_report_flip(mpv_gl, 0);
}

void MpvObject::cleanup()
{
    if (renderer) {
        delete renderer;
        renderer = 0;
    }
}

void MpvObject::on_update(void *ctx)
{
    MpvObject *self = (MpvObject *)ctx;
    emit self->onUpdate();
}

// connected to onUpdate(); signal makes sure it runs on the GUI thread
void MpvObject::doUpdate()
{
    window()->update();
}

void MpvObject::command(const QVariant& params)
{
    mpv::qt::command_variant(mpv, params);
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    // Qt sets the locale in the QGuiApplication constructor, but libmpv
    // requires the LC_NUMERIC category to be set to "C", so change it back.
    std::setlocale(LC_NUMERIC, "C");

    qmlRegisterType<MpvObject>("mpvtest", 1, 0, "MpvObject");

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:///mpvtest/main.qml"));
    view.show();

    return app.exec();
}
