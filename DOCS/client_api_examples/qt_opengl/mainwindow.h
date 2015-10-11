#ifndef MainWindow_H
#define MainWindow_H

#include <QtWidgets/QWidget>

class MpvWidget;
class QSlider;
class QPushButton;
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
public Q_SLOTS:
    void openMedia();
    void seek(int pos);
    void pauseResume();
private Q_SLOTS:
    void setSliderRange(int duration);
private:
    MpvWidget *m_mpv;
    QSlider *m_slider;
    QPushButton *m_openBtn;
    QPushButton *m_playBtn;
};

#endif // MainWindow_H
