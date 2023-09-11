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
    pc/peer_connection.cc \
    global_tmp/global_tmp.cc \
    codec/video_codec.cc \
    transport/receiver/receiver.cc \
    rtp/rtp_handler.cc \
    third_party/tylib/tylib/time/timer.cc \
    rtp/pack_unpack/audio_to_rtp.cc \
    rtp/pack_unpack/rtp_to_vp8.cc

HEADERS  += \
    *.h \
    third_party/tylib/tylib/time/*.h \
    pc/*.h \
    codec/*.h \
    global_tmp/*.h \
    log/*.h \
    transport/receiver/*.h \
    rtp/pack_unpack/*.h \
    rtp/rtcp/*.h \
    rtp/*.h

FORMS    += \
    mainwindow.ui


macx {
INCLUDEPATH += /opt/homebrew/Cellar/ffmpeg/6.0/include \
        $$PWD/third_party/tylib

LIBS += -L /opt/homebrew/Cellar/ffmpeg/6.0/lib \
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
        $$PWD/third_party/ffmpeg/lib/swscale.lib
}
