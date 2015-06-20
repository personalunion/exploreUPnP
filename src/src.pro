QT       += core network xml gui

TARGET = exploreUPnP
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

#LIBS += "/home/user/simon/code/exploreBonjour/miniupnp/libminiupnpc.a" #-lminiupnpc
#INCLUDEPATH += "home/user/simon/code/exploreBonjour/miniupnp"
include(../miniupnp/miniupnp.pri)


SOURCES += main.cpp \
	   extendedupnp.cpp \
    parser.cpp \
    UPnPHandler.cpp
HEADERS += \
	extendedupnp.h \
    parser.h \
    UPnPHandler.h

OTHER_FILES += \
    soapData.xml

RESOURCES += \
    soap.qrc
