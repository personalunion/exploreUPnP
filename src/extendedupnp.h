#ifndef EXTENDEDUPNP_H
#define EXTENDEDUPNP_H


#include "UPnPHandler.h"

#include <miniupnpc/miniupnpc.h>
#include <QMetaEnum>
#include <QObject>
#include <QNetworkReply>

/*
 * Url for elasticsearch database running on port 9200
 * with the index upnp and the type
 * u_entry:
 * "http://localhost:9200/upnp/u_entry"
 */

static const QString _db_url = "http://localhost:9200/upnp/u_entry";

class extendedUPnP : public QObject
{
    Q_OBJECT
    Q_ENUMS(DataType)

public:
    extendedUPnP(QObject *parent = 0);
    ~extendedUPnP();

    enum DataType
    {
        ExternalIpAddress,
        LanIpAddress,
        LinkLayerMaxUpload,
        LinkLayerMaxDownload,
        TotalBytesSent,
        TotalBytesReceived,
        TotalPacketsSent,
        TotalPacketsReceived,
        ConnectionType,
        NatType,
        Status,
        Uptime,
        LastConnectionError,
        NumberOfPortMappings,
        FirewallEnabled,
        InboundPinholeAllowed,
        ModelName,
        Manufacturer,
        FriendlyName,
        ControlUrl,
        EventSubUrl,
        ScpdUrl,
        ServiceType,
        FoundContent,
        URL,
        RootDescUrl
    };
    typedef QHash<DataType, QVariant> UPnPHash;

    bool start();
    bool stop();
    QList<UPnPHash> quickDevicesCheck(UPNPDev * list);
    void printResultsToMap(QVariantList *list);

signals:
    void done();
    void finished();

public slots:
    void result();
    void readAnswer();

private:
    QList<UPnPHash> results;
    QVariantList additional_res;
    QNetworkReply *_rep;

    bool m_mediaServerSearch;
    UPnPHandler * m_handler;

#define enumToString(className, enumName, value) \
    className::staticMetaObject.enumerator(className::staticMetaObject.indexOfEnumerator(#enumName)).key(value)
};

#endif // EXTENDEDUPNP_H
