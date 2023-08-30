#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QThread>
#include <QImage>

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/pixfmt.h"
    #include "libswscale/swscale.h"
}

class VideoPlayer : public QThread
{
    Q_OBJECT

public:
    explicit VideoPlayer(){}
    ~VideoPlayer(){}

    void startPlay(QString url);

signals:
    void sig_GetOneFrame(QImage); //每获取到一帧图像 就发送此信号

protected:
    void run();

private:
    QString m_strFileName;
};

#endif // VIDEOPLAYER_H
