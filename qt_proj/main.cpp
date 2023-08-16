#include "hello.h"


#include <QApplication>

#include <QMediaPlayer>
#include <QtNetwork>

#include <QVideoWidget>
#include <QAudioOutput>

void network()
{
    QString localHostName = QHostInfo::localHostName();
    qDebug() <<"localHostName: "<<localHostName;

    QHostInfo info = QHostInfo::fromName(localHostName);
    qDebug() <<"IP Address: "<<info.addresses();

    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    foreach (QHostAddress address, list)
    {
        //我们使用IPv4地址
        if(address.protocol() == QAbstractSocket::IPv4Protocol)
            qDebug() << "ip=" << address.toString();
    }
}

class UdpServer : public QWidget {

private:
    QUdpSocket * udpSocket_ =nullptr;

public:

    void readPendingDatagrams()
    {
        QHostAddress addr; //用于获取发送者的 IP 和端口
        quint16 port;

        //数据缓冲区
        QByteArray arr;
        while(udpSocket_->hasPendingDatagrams())
        {
            //调整缓冲区的大小和收到的数据大小一致
            arr.resize(udpSocket_->bytesAvailable());

            //接收数据
            udpSocket_->readDatagram(arr.data(),arr.size(),&addr,&port);
            //显示
            // QString my_formatted_string = QString("%1/%2-%3.txt").arg("~", 2232, "Jane");

            // qDebug() << QString("recv from %1:%2, msg=%3.").arg(addr.toString().arg(port).arg(arr));
            qDebug() << addr.toString() << ":" <<port << "=" << QString( arr);
        }
        qDebug() << "exit while reading";
    }

    void udpSend() {
        udpSocket_ = new QUdpSocket();

        bool result =  udpSocket_->bind(QHostAddress::AnyIPv4);
        if(result)
        {
            qDebug() << "Socket PASS";
        }
        else
        {
            qDebug() << "Socket FAIL";
        }

        // Send it out to some IP (192.168.1.1) and port (45354).
        qint64 bytesSent = udpSocket_->writeDatagram(QByteArray("Hello Qt!"), QHostAddress("36.155.205.204"), 8090);
        qDebug() << "send size is " << bytesSent;

        connect(udpSocket_, &QUdpSocket::readyRead, this, &UdpServer::readPendingDatagrams);
    }

};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //Dialog w;
    //w.show();

    QMediaPlayer *player= new QMediaPlayer;
    QVideoWidget * vw = new QVideoWidget;
    player->setVideoOutput(vw);

#ifdef Q_OS_MACX
    player->setSource(QUrl::fromLocalFile("/Users/taylorjiang/big.mp4"));
#elif defined(Q_OS_WIN)
    player->setSource(QUrl::fromLocalFile("C:\\Users\\jiang\\Downloads\\big.mp4"));
#else
#error "not support os"
#endif

    QAudioOutput* audioOutput = new QAudioOutput;
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(50);

    vw->setGeometry(100, 100, 500, 500);
    vw->show();
    player->play();
    qDebug() << "hello=" <<  player->playbackState();

    network();

    UdpServer svr;
    svr.udpSend();

    return a.exec();
}
