﻿#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QImage>
#include <QThread>

class VideoPlayer : public QThread {
  Q_OBJECT

 public:
  explicit VideoPlayer() {}
  ~VideoPlayer() {}

  void startPlay(QString url);

 signals:
  void sig_GetOneFrame(QImage);  //每获取到一帧图像 就发送此信号

 protected:
  void run();

 private:
  QString m_strFileName;  // no use
};

extern VideoPlayer* g_pPlayer;

#endif  // VIDEOPLAYER_H
