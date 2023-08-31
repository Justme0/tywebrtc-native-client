#include "videoplayer.h"

#include <cstdio>
#include <iostream>

#include <QtNetwork>

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/pixfmt.h"
    #include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

using namespace std;

// TODO: should save to remote DB ? must refactor! Now we use singleton
// OPT2: move to tylib
template <class T>
class Singleton : private T {
public:
    // may have param
    static T& Instance() {
        // 1. C++11: If control enters the declaration concurrently while the
        // variable is being initialized, the concurrent execution shall wait for
        // completion of the initialization.
        // 2. Lazy evaluation.
        static Singleton<T> s;

        return s;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

private:
    Singleton() {}
    ~Singleton() {}
};

class UdpServer : public QThread {
public:
    UdpServer() {
        udpSocket_ = new QUdpSocket();

        bool result =  udpSocket_->bind(QHostAddress::AnyIPv4);
        if(result)
        {
            qDebug() << "Socket bind ok";
        }
        else
        {
            qDebug() << "Socket bind fail";
            assert(!"shit");
        }

        connect(udpSocket_, &QUdpSocket::readyRead, this, &UdpServer::readPendingDatagrams);
    }

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

    void sendPing() {
        // Send it out to some IP (192.168.1.1) and port (45354).
        qint64 bytesSent = udpSocket_->writeDatagram(QByteArray("Hello Qt!"), QHostAddress("36.155.205.204"), 8090);
        qDebug() << "send size is " << bytesSent;
    }

private:
    QUdpSocket * udpSocket_ =nullptr;
};

void MyPing() {
    Singleton<UdpServer>::Instance().sendPing();
}

void VideoPlayer::startPlay(QString url)
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MyPing);
    timer->start(1000);

    //调用start函数将会自动执行run函数
    m_strFileName = url;
    this->start();
}

void VideoPlayer::run()
{
    AVFormatContext *pFormatCtx; //音视频封装格式上下文结构体
    AVCodecContext  *pCodecCtx;  //音视频编码器上下文结构体
    const AVCodec *pCodec; //音视频编码器结构体
    AVFrame *pFrame; //存储一帧解码后像素数据
    AVFrame *pFrameRGB;
    AVPacket *pPacket; //存储一帧压缩编码数据
    uint8_t *pOutBuffer;
    static struct SwsContext *pImgConvertCtx;

    avformat_network_init();   //初始化FFmpeg网络模块

    //Allocate an AVFormatContext.
    pFormatCtx = avformat_alloc_context();

    //AVDictionary
    AVDictionary *avdic=nullptr;
    char option_key[]="rtsp_transport";
    char option_value[]="udp";
    av_dict_set(&avdic,option_key,option_value,0);
    char option_key2[]="max_delay";
    char option_value2[]="100";
    av_dict_set(&avdic,option_key2,option_value2,0);

    if (avformat_open_input(&pFormatCtx, m_strFileName.toLocal8Bit().data(), nullptr, &avdic) != 0)
    {
        printf("can't open the file. \n");
        return;
    }

    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0)
    {
        printf("Could't find stream infomation.\n");
        return;
    }

    //查找视频中包含的流信息，音频流先不处理
    int videoStreamIdx = -1;
    qDebug("apFormatCtx->nb_streams:%d", pFormatCtx->nb_streams);
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIdx = i; //视频流
        }
    }
    if (videoStreamIdx == -1)
    {
        printf("Didn't find a video stream.\n"); //没有找到视频流
        return;
    }

    // old and new API diff: https://www.cnblogs.com/schips/p/12197116.html
    //查找解码器
    qDebug("avcodec_find_decoder...");

    pCodecCtx = avcodec_alloc_context3(NULL);

    avcodec_parameters_to_context(pCodecCtx,pFormatCtx->streams[videoStreamIdx]->codecpar);

    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == nullptr)
    {
        printf("Codec not found.\n");
        return;
    }
    pCodecCtx->bit_rate =0;   //初始化为0
    pCodecCtx->time_base.num=1;  //下面两行：一秒钟25帧
    pCodecCtx->time_base.den=10;
    pCodecCtx->frame_number=1;  //每包一个视频帧

    //打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0)
    {
        printf("Could not open codec.\n");
        return;
    }

    //将解码后的YUV数据转换成RGB32
    pImgConvertCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                     pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                                     AV_PIX_FMT_RGB32, SWS_BICUBIC, nullptr, nullptr, nullptr);

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32, pCodecCtx->width,pCodecCtx->height, 1);

    pFrame     = av_frame_alloc();
    pFrameRGB  = av_frame_alloc();
    pOutBuffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, pOutBuffer, AV_PIX_FMT_RGB32, pCodecCtx->width, pCodecCtx->height, 1);

    pPacket = (AVPacket *) malloc(sizeof(AVPacket)); //分配一个packet
    int y_size = pCodecCtx->width * pCodecCtx->height;
    av_new_packet(pPacket, y_size); //分配packet的数据

    while (1)
    {
        if (av_read_frame(pFormatCtx, pPacket) < 0)
        {
            break; //这里认为视频读取完了
        }

        if (pPacket->stream_index == videoStreamIdx)
        {
            int got_picture;
            //int ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture,pPacket);
            int ret = avcodec_send_packet(pCodecCtx, pPacket);
            if (ret )
            {
                printf("decode error.\n");
                return;
            }
            got_picture = 0 == avcodec_receive_frame(pCodecCtx, pFrame); //got_picture = 0 success, a frame was returned

            if (got_picture)
            {
                sws_scale(pImgConvertCtx, (uint8_t const * const *) pFrame->data, pFrame->linesize,
                          0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

                //把这个RGB数据 用QImage加载
                QImage tmpImg((uchar *)pOutBuffer, pCodecCtx->width, pCodecCtx->height, QImage::Format_RGB32);
                QImage image = tmpImg.copy(); //把图像复制一份 传递给界面显示
                emit sig_GetOneFrame(image);  //发送信号
            }
        }
        av_packet_unref(pPacket);
        //msleep(0.02);
    }

    av_free(pOutBuffer);
    av_free(pFrameRGB);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
}
