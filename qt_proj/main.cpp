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

void udpSend() {
    QUdpSocket *udpSocket = new QUdpSocket();

    bool result =  udpSocket->bind(QHostAddress::AnyIPv4, 21939);
    if(result)
    {
        qDebug() << "Socket PASS";
    }
    else
    {
        qDebug() << "Socket FAIL";
    }

    // Send it out to some IP (192.168.1.1) and port (45354).
    qint64 bytesSent = udpSocket->writeDatagram(QByteArray("Hello Qt!"),
                                            QHostAddress("36.155.205.204"),
                                            8090);
    qDebug() << "send size is " << bytesSent;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //Dialog w;
    //w.show();

    QMediaPlayer *player= new QMediaPlayer;
    QVideoWidget * vw = new QVideoWidget;
    player->setVideoOutput(vw);
    //player->setSource(QUrl::fromLocalFile("C:\\Users\\jiang\\Downloads\\big.mp4"));
     // player->setSource(QUrl::fromLocalFile("/Users/taylorjiang/big.mp4"));
     player->setSource(QUrl::fromLocalFile("C:\\Users\\jiang\\Downloads\\big.mp4"));

    QAudioOutput* audioOutput = new QAudioOutput;
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(50);

    vw->setGeometry(100, 100, 500, 500);
    vw->show();
    player->play();
    qDebug() << "hello=" <<  player->playbackState();

    network();

    udpSend();

    return a.exec();
}
