

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QPaintEvent>
#include <QWidget>
#include <QtDebug>

#include <QtConcurrent/qtconcurrentrun.h>
#include "videoplayer.h"

enum RTSP_PLAY_STATE
{
    RPS_IDLE,    //空闲
    RPS_PREPARE, //连接中
    RPS_RUNNING, //播放中
    RPS_PAUSE    //暂停中
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int playVideo(char* videoPath);

protected:
    void paintEvent(QPaintEvent *event);

private:
    Ui::MainWindow *ui;

    VideoPlayer *m_pPlayer;
    QImage m_Image; //记录当前的图像
    QString m_strUrl;

    RTSP_PLAY_STATE m_kPlayState = RPS_IDLE;
    bool m_bHFlip = false;
    bool m_bVFlip = false;

private slots:
    void slotGetOneFrame(QImage img);
    void on_pushButton_toggled(bool checked);
    void on_checkBoxVFlip_clicked(bool checked);
    void on_checkBoxHFlip_clicked(bool checked);
};

#endif // MAINWINDOW_H
