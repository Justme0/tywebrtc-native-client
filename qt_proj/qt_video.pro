#-------------------------------------------------
#
# Project created by QtCreator
#
#-------------------------------------------------

QT       += core gui core5compat network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# TARGET = rtspPlayer
TEMPLATE = app


SOURCES += \
    *.cpp \
    codec/video_codec.cc \
    global_tmp/global_tmp.cc \
    pc/peer_connection.cc \
    rtp/pack_unpack/audio_to_rtp.cc \
    rtp/pack_unpack/rtp_to_h264.cc \
    rtp/rtp_handler.cc \
    third_party/tylib/tylib/time/timer.cc \
    transport/receiver/receiver.cc \
    rtp/pack_unpack/rtp_to_vp8.cc

HEADERS  += \
    *.h \
    codec/*.h \
    global_tmp/*.h \
    log/*.h \
    pc/*.h \
    rtp/pack_unpack/*.h \
    rtp/rtcp/*.h \
    third_party/tylib/tylib/time/*.h \
    transport/receiver/*.h \
    transport/sender/*.h \
    rtp/*.h

FORMS    += \
    mainwindow.ui


macx {
INCLUDEPATH += /opt/homebrew/Cellar/ffmpeg/6.0_1/include \
        $$PWD/third_party/tylib

LIBS += -L /opt/homebrew/Cellar/ffmpeg/6.0_1/lib \
        -lavcodec \
        -lavdevice \
        -lavfilter \
        -lavformat \
        -lavutil \
        -lpostproc \
        -lswscale \
        -lswresample
} win32 {
INCLUDEPATH+=$$PWD/third_party/ffmpeg/include \
        $$PWD/third_party/tylib

LIBS += $$PWD/third_party/ffmpeg/lib/avcodec.lib \
        $$PWD/third_party/ffmpeg/lib/avdevice.lib \
        $$PWD/third_party/ffmpeg/lib/avfilter.lib \
        $$PWD/third_party/ffmpeg/lib/avformat.lib \
        $$PWD/third_party/ffmpeg/lib/avutil.lib \
        $$PWD/third_party/ffmpeg/lib/postproc.lib \
        $$PWD/third_party/ffmpeg/lib/swresample.lib \
        $$PWD/third_party/ffmpeg/lib/swscale.lib \
        -l ws2_32
}
