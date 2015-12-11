#include "mainwindow.h"
#include "mpvwidget.h"
#include <QPushButton>
#include <QSlider>
#include <QLayout>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    m_mpv = new MpvWidget(this);
    m_slider = new QSlider();
    m_slider->setOrientation(Qt::Horizontal);
    m_openBtn = new QPushButton("Open");
    m_playBtn = new QPushButton("Pause");
    QHBoxLayout *hb = new QHBoxLayout();
    hb->addWidget(m_openBtn);
    hb->addWidget(m_playBtn);
    QVBoxLayout *vl = new QVBoxLayout();
    vl->addWidget(m_mpv);
    vl->addWidget(m_slider);
    vl->addLayout(hb);
    setLayout(vl);
    connect(m_slider, SIGNAL(sliderMoved(int)), SLOT(seek(int)));
    connect(m_openBtn, SIGNAL(clicked()), SLOT(openMedia()));
    connect(m_playBtn, SIGNAL(clicked()), SLOT(pauseResume()));
    connect(m_mpv, SIGNAL(positionChanged(int)), m_slider, SLOT(setValue(int)));
    connect(m_mpv, SIGNAL(durationChanged(int)), this, SLOT(setSliderRange(int)));
}

void MainWindow::openMedia()
{
    QString file = QFileDialog::getOpenFileName(0, "Open a video");
    if (file.isEmpty())
        return;
    m_mpv->command(QStringList() << "loadfile" << file);
}

void MainWindow::seek(int pos)
{
    m_mpv->command(QVariantList() << "seek" << pos << "absolute");
}

void MainWindow::pauseResume()
{
    const bool paused = m_mpv->getProperty("pause").toBool();
    m_mpv->setProperty("pause", !paused);
}

void MainWindow::setSliderRange(int duration)
{
    m_slider->setRange(0, duration);
}
