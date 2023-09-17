#include "videoplayer.h"

#include <QtNetwork>
#include <cstdio>
#include <iostream>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

#include "pc/peer_connection.h"

class UdpServer : public QThread {
 public:
  UdpServer() {
    udpSocket_ = new QUdpSocket();

    bool result = udpSocket_->bind(QHostAddress::AnyIPv4);
    if (result) {
      tylog("Socket bind ok");
    } else {
      tylog("Socket bind fail");
      assert(!"shit");
    }

    connect(udpSocket_, &QUdpSocket::readyRead, this,
            &UdpServer::readPendingDatagrams);
  }

  void readPendingDatagrams() {
    QHostAddress addr;  //用于获取发送者的 IP 和端口
    quint16 port;

    //数据缓冲区
    QByteArray arr;
    while (udpSocket_->hasPendingDatagrams()) {
      //调整缓冲区的大小和收到的数据大小一致
      arr.resize(udpSocket_->bytesAvailable());

      //接收数据
      udpSocket_->readDatagram(arr.data(), arr.size(), &addr, &port);
      //显示
      // QString my_formatted_string = QString("%1/%2-%3.txt").arg("~", 2232,
      // "Jane");

      tylog("recv from %s:%d, size=%d.", addr.toString().toStdString().data(),
            port, arr.size());

      static PeerConnection pc(addr.toString().toStdString(), (int)port);

      pc.HandlePacket(std::vector<char>(arr.begin(), arr.end()));
    }
    tylog("exit while reading");
  }

  void sendPing() {
    // Send it out to some IP (192.168.1.1) and port (45354).
    qint64 bytesSent = udpSocket_->writeDatagram(
        QByteArray("Hello Qt!"), QHostAddress("36.155.205.204"), 8090);
    tylog("send size is %ld", bytesSent);
  }

 private:
  QUdpSocket *udpSocket_ = nullptr;
};

void MyPing() { Singleton<UdpServer>::Instance().sendPing(); }

void VideoPlayer::startPlay(QString url) {
  QTimer *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &MyPing);
  timer->start(1000);

  //调用start函数将会自动执行run函数
  m_strFileName = url;
  this->start();
}

void VideoPlayer::run() {}
