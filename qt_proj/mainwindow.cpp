#include "mainwindow.h"

#include <QInputDialog>
#include <QPainter>
#include <QThread>
#include <QTime>
#include <QtMath>
#include <iostream>

#include "log/log.h"
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

  tylog("ffmpeg version=%u, config=%s.", avcodec_version(),
        avcodec_configuration());

  g_pPlayer = new VideoPlayer;
  connect(g_pPlayer, SIGNAL(sig_GetOneFrame(QImage)), this,
          SLOT(slotGetOneFrame(QImage)));

  ui->lineEditUrl->setText("");

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
