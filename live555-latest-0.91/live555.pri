
INCLUDEPATH += $$PWD/live/BasicUsageEnvironment/include \
    $$PWD/live/groupsock/include \
    $$PWD/live/liveMedia/include \
    $$PWD/live/UsageEnvironment/include
LIBS += -lws2_32 -lwsock32

INCLUDEPATH += $$PWD/ffmpeg-20180411-9825f77-win32-dev/include
LIBS += -L$$PWD/ffmpeg-20180411-9825f77-win32-dev/lib \
        -lavcodec   \
        -lavdevice  \
        -lavfilter  \
        -lavformat  \
        -lavutil    \
        -lpostproc  \
        -lswresample\
        -lswscale

QMAKE_CXXFLAGS += -D__MINGW32__ -Wall -Wno-deprecated -Wno-deprecated-declarations
QMAKE_CFLAGS += -DUSE_OUR_BZERO=1 -D__MINGW32__ -Wno-deprecated-declarations -Wno-unused-parameter

include($$PWD/live/liveMedia/liveMedia.pri)
include($$PWD/live/BasicUsageEnvironment/BasicUsageEnvironment.pri)
include($$PWD/live/groupsock/groupsock.pri)
include($$PWD/live/UsageEnvironment/UsageEnvironment.pri)
