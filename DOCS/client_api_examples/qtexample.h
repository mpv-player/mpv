#ifndef QTEXAMPLE_H
#define QTEXAMPLE_H

#include <QMainWindow>

#include <mpv/client.h>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    virtual bool event(QEvent *event);

private slots:
    void on_file_open();

private:
    QWidget *mpv_container;
    mpv_handle *mpv;

    void create_player();
    void handle_mpv_event(mpv_event *event);
};

#endif // QTEXAMPLE_H
