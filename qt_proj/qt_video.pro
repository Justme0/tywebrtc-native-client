#-------------------------------------------------
#
# Project created by QtCreator
#
#-------------------------------------------------

QT       += core gui core5compat network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# TARGET = rtspPlayer
TEMPLATE = app


SOURCES += main.cpp \
    videoplayer.cpp \
    mainwindow.cpp

HEADERS  += \
    videoplayer.h \
    mainwindow.h

FORMS    += \
    mainwindow.ui

macx {
INCLUDEPATH += /opt/homebrew/Cellar/ffmpeg/6.0/include

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
INCLUDEPATH+=$$PWD/third_party/ffmpeg/include

LIBS += $$PWD/third_party/ffmpeg/lib/avcodec.lib \
        $$PWD/third_party/ffmpeg/lib/avdevice.lib \
        $$PWD/third_party/ffmpeg/lib/avfilter.lib \
        $$PWD/third_party/ffmpeg/lib/avformat.lib \
        $$PWD/third_party/ffmpeg/lib/avutil.lib \
        $$PWD/third_party/ffmpeg/lib/postproc.lib \
        $$PWD/third_party/ffmpeg/lib/swresample.lib \
        $$PWD/third_party/ffmpeg/lib/swscale.lib
}
