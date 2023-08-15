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

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //Dialog w;
    //w.show();

    QMediaPlayer *player= new QMediaPlayer;
    QVideoWidget * vw = new QVideoWidget;
    player->setVideoOutput(vw);
    //player->setSource(QUrl::fromLocalFile("C:\\Users\\jiang\\Downloads\\big.mp4"));
     player->setSource(QUrl::fromLocalFile("/Users/taylorjiang/big.mp4"));

    QAudioOutput* audioOutput = new QAudioOutput;
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(50);

    vw->setGeometry(100, 100, 500, 500);
    vw->show();
    player->play();
    qDebug() << "hello=" <<  player->playbackState();

    network();

    return a.exec();
}
