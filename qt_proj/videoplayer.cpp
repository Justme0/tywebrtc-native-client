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
    udpSocket_ = new QUdpSocket(this);

    qint64 bufferByte = 64 * 1024 * 1024;

    udpSocket_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                                bufferByte);

    bool result = udpSocket_->bind(QHostAddress::Any);
    if (result) {
      tylog("Socket bind ok");
    } else {
      tylog("Socket bind fail");
      assert(!"shit");
    }

    bool ok = connect(udpSocket_, &QUdpSocket::readyRead, this,
                      &UdpServer::readPendingDatagrams);
    if (!ok) {
      assert(!"connect not ok :( , bye");
    }

    ok = connect(udpSocket_, &QUdpSocket::errorOccurred, this,
                 &UdpServer::onError);
    if (!ok) {
      assert(!"connect not ok :( , bye");
    }
  }

  void onError(QAbstractSocket::SocketError socketError) {
    tylog("udp socket err=%d.", socketError);
    assert(!"connect recv error :( , bye");
  }

  void readPendingDatagrams() {
    while (udpSocket_->hasPendingDatagrams()) {
      tylog("udpSocket_->bytesAvailable=%" PRId64
            ", first pending datagram size=%" PRId64 ".",
            udpSocket_->bytesAvailable(), udpSocket_->pendingDatagramSize());

      QNetworkDatagram datagram = udpSocket_->receiveDatagram();
      if (!datagram.isValid()) {
        tylog("recv not valid");
        assert(!"recv invalid");
      }

      QByteArray rawData = datagram.data();  // shallow copy

      tylog("==== recv %s:%d->%s:%d, TTL=%d, size=%lld ====",
            datagram.senderAddress().toString().toStdString().data(),
            datagram.senderPort(),
            datagram.destinationAddress().toString().toStdString().data(),
            datagram.destinationPort(), datagram.hopLimit(), rawData.size());

      static PeerConnection pc(
          datagram.senderAddress().toString().toStdString(),
          datagram.senderPort());

      pc.HandlePacket(std::vector<char>(rawData.begin(), rawData.end()));
    }
    tylog("exit while reading");
  }

  void sendPing() {
    // Send it out to some IP (192.168.1.1) and port (45354).
    qint64 bytesSent = udpSocket_->writeDatagram(
        QByteArray("Hello Qt!"), QHostAddress("36.155.205.204"), 8090);
    tylog("send size is %lld", bytesSent);
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
