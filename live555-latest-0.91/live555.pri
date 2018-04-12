
INCLUDEPATH += $$PWD/live/BasicUsageEnvironment/include \
    $$PWD/live/groupsock/include \
    $$PWD/live/liveMedia/include \
    $$PWD/live/UsageEnvironment/include
win32:LIBS += -lws2_32 -lwsock32

INCLUDEPATH += /home/xb/opensource/FFmpegAndDepend/ffmpeg-2.0.2/_install_pc/include
LIBS += -L/home/xb/opensource/FFmpegAndDepend/ffmpeg-2.0.2/_install_pc/lib \
        -lavcodec   \
        -lavdevice  \
        -lavfilter  \
        -lavformat  \
        -lavutil    \
        -lswresample\
        -lswscale

QMAKE_CXXFLAGS += -O2 -DSOCKLEN_T=socklen_t -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -Wall -DBSD=1
QMAKE_CFLAGS += -O2 -DSOCKLEN_T=socklen_t -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -Wall -DBSD=1

include($$PWD/live/liveMedia/liveMedia.pri)
include($$PWD/live/BasicUsageEnvironment/BasicUsageEnvironment.pri)
include($$PWD/live/groupsock/groupsock.pri)
include($$PWD/live/UsageEnvironment/UsageEnvironment.pri)
