#include "mainwindow.h"

#include <QInputDialog>
#include <QPainter>
#include <QThread>
#include <QTime>
#include <QtMath>
#include <iostream>

#include "ui_mainwindow.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#pragma execution_character_set("utf-8")

using namespace std;

VideoPlayer *g_pPlayer;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  qDebug("ffmpeg version=%u, config=%s.", avcodec_version(),
         avcodec_configuration());

  g_pPlayer = new VideoPlayer;
  connect(g_pPlayer, SIGNAL(sig_GetOneFrame(QImage)), this,
          SLOT(slotGetOneFrame(QImage)));

  // m_strUrl = "rtsp://192.168.5.100:8554/vlc";
  // m_strUrl = "rtsp://192.168.5.154:8554/test.h264";
  // m_strUrl =
  // "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4";
  // m_strUrl =
  // "rtsp://zephyr.rtsp.stream/movie?streamKey=324260c63242294fdd6df91587d87e50";
  m_strUrl =
      "rtmp://rtmp.rtc.qq.com/pull/"
      "295036?sdkappid=1400188366&userid=taylorobs&usersig=eJw8jl8LgjAUR7-"
      "LfQ67m23ZoJegJJRAVlK9Wa649kfTUYvou0cmvf4OnN95wTLWnnEV1QYUE0JwROy1K*"
      "XmaulApgYFNnuey7rcNfCDTX7KqopyUGyAyILAl7IjdAQFDxnzIhGb8uaKhZumYaqDbWKiSb"
      "R3a4t9zfksDJmv56txp7R0*TbIEWdD9tfd23vuIbw-AQAA---IVDJ0&use_number_room_"
      "id=1&remoteuserid=taylorobs";
  ui->lineEditUrl->setText(m_strUrl);

  g_pPlayer->startPlay("");
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::slotGetOneFrame(QImage img) {
  ui->labelCenter->clear();
  if (m_kPlayState == RPS_PAUSE) {
    return;
  }

  m_Image = img;
  update();  //调用update将执行paintEvent函数
}

void MainWindow::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  int showWidth = this->width() - 100;
  int showHeight = this->height() - 50;

  painter.setBrush(Qt::white);
  painter.drawRect(0, 0, this->width(), this->height());  //先画成白色

  if (m_Image.size().width() <= 0) {
    return;
  }

  //将图像按比例缩放
  QImage img =
      m_Image.scaled(QSize(showWidth, showHeight), Qt::KeepAspectRatio);
  img = img.mirrored(m_bHFlip, m_bVFlip);

  int x = this->width() - img.width();
  int y = this->height() - img.height();
  x /= 2;
  y /= 2;

  painter.drawImage(QPoint(x - 40, y + 20), img);  //画出图像
}

void MainWindow::on_pushButton_toggled(bool checked) {
  if (checked)  //第一次按下为启动，后续则为继续
  {
    if (m_kPlayState == RPS_IDLE) {
      ui->lineEditUrl->setEnabled(false);
      m_strUrl = ui->lineEditUrl->text();
      g_pPlayer->startPlay(m_strUrl);

      ui->labelCenter->setText("rtsp网络连接中...");
    }
    m_kPlayState = RPS_RUNNING;
    ui->pushButton->setText("暂停");
  } else {
    m_kPlayState = RPS_PAUSE;
    ui->pushButton->setText("播放");
  }
}

void MainWindow::on_checkBoxVFlip_clicked(bool checked) { m_bVFlip = checked; }

void MainWindow::on_checkBoxHFlip_clicked(bool checked) { m_bHFlip = checked; }
