TEMPLATE = lib
CONFIG += staticlib
CONFIG -= qt

!win32 {
    strings.target = miniupnpc/miniupnpcstrings.h
    strings.commands = cd $$PWD/miniupnpc && sh updateminiupnpcstrings.sh
    strings.depends = FORCE

    QMAKE_EXTRA_TARGETS += strings
    PRE_TARGETDEPS += miniupnpc/miniupnpcstrings.h
}

DEFINES += HAVE_UPNP MINIUPNP_STATICLIB USE_GETHOSTBYNAME MINIUPNPC_SET_SOCKET_TIMEOUT

# Disable warnings
*clang*|*g++*|*llvm* {
    QMAKE_CFLAGS += -w
    QMAKE_CXXFLAGS += -w
}

#android:DEFINES += sun # Atleast needed for android
linux:DEFINES += _GNU_SOURCE
#mac:DEFINES += _DARWIN_C_SOURCE
#win32:DEFINES += _CRT_SECURE_NO_WARNINGS

android|mac|win32|linux {
    INCLUDEPATH += $$PWD
    # win32:INCLUDEPATH += $$PWD/miniupnp_windows_fixes

    HEADERS += \
        $$PWD/miniupnpc/bsdqueue.h \
        $$PWD/miniupnpc/codelength.h \
        $$PWD/miniupnpc/connecthostport.h \
        $$PWD/miniupnpc/declspec.h \
        $$PWD/miniupnpc/igd_desc_parse.h \
        $$PWD/miniupnpc/minisoap.h \
        $$PWD/miniupnpc/minissdpc.h \
        $$PWD/miniupnpc/miniupnpc.h \
        $$PWD/miniupnpc/miniupnpctypes.h \
        $$PWD/miniupnpc/miniwget.h \
        $$PWD/miniupnpc/minixml.h \
        $$PWD/miniupnpc/portlistingparse.h \
        $$PWD/miniupnpc/receivedata.h \
        $$PWD/miniupnpc/upnpcommands.h \
        $$PWD/miniupnpc/upnperrors.h \
        $$PWD/miniupnpc/upnpreplyparse.h


    SOURCES += \
        $$PWD/miniupnpc/connecthostport.c \
        $$PWD/miniupnpc/igd_desc_parse.c \
        $$PWD/miniupnpc/minisoap.c \
        $$PWD/miniupnpc/minissdpc.c \
        $$PWD/miniupnpc/miniupnpc.c \
        $$PWD/miniupnpc/miniwget.c \
        $$PWD/miniupnpc/minixml.c \
        $$PWD/miniupnpc/portlistingparse.c \
        $$PWD/miniupnpc/receivedata.c \
        $$PWD/miniupnpc/upnpcommands.c \
        $$PWD/miniupnpc/upnperrors.c \
        $$PWD/miniupnpc/upnpreplyparse.c
}

OTHER_FILES = miniupnp.pri
