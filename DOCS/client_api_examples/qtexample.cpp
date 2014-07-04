// License: pick one of: public domain, WTFPL, ISC, Ms-PL, AGPLv3

// This example can be built with: qmake && make

#include <sstream>

#include <QFileDialog>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QGridLayout>
#include <QApplication>

#include "qtexample.h"

static void wakeup(void *ctx)
{
    // This callback is invoked from any mpv thread (but possibly also
    // recursively from a thread that is calling the mpv API). Just notify
    // the Qt GUI thread to wake up (so that it can process events with
    // mpv_wait_event()), and return as quickly as possible.
    MainWindow *mainwindow = (MainWindow *)ctx;
    QCoreApplication::postEvent(mainwindow, new QEvent(QEvent::User));
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
    QMenu *menu = menuBar()->addMenu(tr("&File"));
    QAction *on_open = new QAction(tr("&Open"), this);
    on_open->setShortcuts(QKeySequence::Open);
    on_open->setStatusTip(tr("Open a file"));
    connect(on_open, SIGNAL(triggered()), this, SLOT(on_file_open()));
    menu->addAction(on_open);

    statusBar();

    mpv = mpv_create();
    if (!mpv)
        throw "can't create mpv instance";

    // Create a video child window. Force Qt to create a native window, and
    // pass the window ID to the mpv wid option. This doesn't work on OSX,
    // because Cocoa doesn't support this form of embedding.
    mpv_container = new QWidget(this);
    setCentralWidget(mpv_container);
    mpv_container->setAttribute(Qt::WA_NativeWindow);
    int64_t wid = mpv_container->winId();
    mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);

    // Enable default bindings, because we're lazy. Normally, a player using
    // mpv as backend would implement its own key bindings.
    mpv_set_option_string(mpv, "input-default-bindings", "yes");

    // Let us receive property change events with MPV_EVENT_PROPERTY_CHANGE if
    // this property changes.
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);

    // From this point on, the wakeup function will be called. The callback
    // can come from any thread, so we use the Qt QEvent mechanism to relay
    // the wakeup in a thread-safe way.
    mpv_set_wakeup_callback(mpv, wakeup, this);

    if (mpv_initialize(mpv) < 0)
        throw "mpv failed to initialize";
}

void MainWindow::handle_mpv_event(mpv_event *event)
{
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = (mpv_event_property *)event->data;
        if (strcmp(prop->name, "time-pos") == 0) {
            if (prop->format == MPV_FORMAT_DOUBLE) {
                double time = *(double *)prop->data;
                std::stringstream ss;
                ss << "At: " << time;
                statusBar()->showMessage(QString::fromStdString(ss.str()));
            } else if (prop->format == MPV_FORMAT_NONE) {
                // The property is unavailable, which probably means playback
                // was stopped.
                statusBar()->showMessage("");
            }
        }
        break;
    }
    case MPV_EVENT_SHUTDOWN: {
        mpv_terminate_destroy(mpv);
        mpv = NULL;
        break;
    }
    default: ;
        // Ignore uninteresting or unknown events.
    }
}

bool MainWindow::event(QEvent *event)
{
    // QEvent::User is sent by wakeup().
    if (event->type() == QEvent::User) {
        // Process all events, until the event queue is empty.
        while (mpv) {
            mpv_event *event = mpv_wait_event(mpv, 0);
            if (event->event_id == MPV_EVENT_NONE)
                break;
            handle_mpv_event(event);
        }
        return true;
    }
    return QMainWindow::event(event);
}

void MainWindow::on_file_open()
{
    QString filename = QFileDialog::getOpenFileName(this, "Open file");
    if (mpv) {
        const char *args[] = {"loadfile", filename.toUtf8().data(), NULL};
        mpv_command_async(mpv, 0, args);
    }
}

MainWindow::~MainWindow()
{
    if (mpv)
        mpv_terminate_destroy(mpv);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
