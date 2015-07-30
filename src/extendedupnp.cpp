#include "extendedupnp.h"

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/miniwget.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>

extendedUPnP::extendedUPnP(QObject *parent)
{
    Q_UNUSED(parent)
}

extendedUPnP::~extendedUPnP()
{

}

QStringList GetValsFromNameValueList(struct NameValueParserData *pdata,
                                       const char *Name)
{
    QStringList ret;
    struct NameValue *nv;

    for (nv = pdata->l_head;
         (nv != NULL);
         nv = nv->l_next)
    {
        if (strcmp(nv->name, Name) == 0)
        {
            ret.append(nv->value);
        }
    }

    return ret;
}

bool extendedUPnP::start()
{
    m_handler = new UPnPHandler();
    int error = 0;
    /* The following devices are important */
    static const char * const deviceList[] = {
        "urn:schemas-upnp-org:device:MediaServer:1",
//            "urn:schemas-upnp-org:service:ContentDirectory:1",
        /*"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
        "ssdp:all"*/
        0
    };
    // TODO test other devices from upnp.org
    /* more or less important:
    "urn:schemas-upnp-org:service:ConnectionManager:1",
    "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
    "urn:schemas-upnp-org:service:WANIPConnection:2",
    "urn:schemas-upnp-org:service:WANIPConnection:1",
    "urn:schemas-upnp-org:service:WANPPPConnection:1",
    "upnp:rootdevice",*/
    UPNPDev *devices = upnpDiscoverDevices(deviceList,
                                           2000, NULL, NULL, FALSE,
                                           FALSE, &error);
    QString descriptionUrl, eventSubUrl, controlUrl, serviceType, modelName;
    QList<UPnPHash> mediaServerList = quickDevicesCheck(devices);
    qDebug() << "-- Going through devices --";
    UPnPHash mServer;
    connect(this, SIGNAL(finished()), this, SLOT(result()));
    foreach(mServer, mediaServerList)
    {
        QList<QMap<QString, QString> > resultsOfSniff;
        descriptionUrl = mServer.value(RootDescUrl).toString();
        eventSubUrl = mServer.value(EventSubUrl).toString();
        controlUrl = mServer.value(ControlUrl).toString();
        serviceType = mServer.value(ServiceType).toString();
        modelName = mServer.value(ModelName).toString();
        int ret = m_handler->init(descriptionUrl, eventSubUrl, controlUrl, serviceType);
        if(ret < 0)
        {
            qDebug() << "ret" << ret << "Unsuccessfull on " << modelName << "at" << descriptionUrl;
            if(mServer.value(ControlUrl) != m_handler->actionUrl().path())
            {
                mServer.insert(ControlUrl, m_handler->actionUrl().path());
            }
            if(mServer.value(EventSubUrl) != m_handler->subscribeUrl().path())
            {
                mServer.insert(EventSubUrl, m_handler->subscribeUrl().path());
            }
            if(mServer.value(ServiceType) != m_handler->servicetype())
            {
                mServer.insert(ServiceType, m_handler->servicetype());
            }
            results.append(mServer);
            m_handler->cleanup();
        }else{
            qDebug() << "ret" << ret << "Successfull on " << modelName << "at" << descriptionUrl;
            resultsOfSniff = m_handler->totalTableOfContents();
            printResultsToMap(&additional_res);
            QVariant res(additional_res);
            mServer.insert(FoundContent, res);
            if(mServer.value(ControlUrl) != m_handler->actionUrl().path())
            {
                mServer.insert(ControlUrl, m_handler->actionUrl().path());
            }
            if(mServer.value(EventSubUrl) != m_handler->subscribeUrl().path())
            {
                mServer.insert(EventSubUrl, m_handler->subscribeUrl().path());
            }
            results.append(mServer);
            m_handler->cleanup();
        }
        ret = 0;
    }
    emit finished();
    return true;
}

bool extendedUPnP::stop()
{
    return true;
}

void extendedUPnP::result()
{
   QString formattedDateTime = QDateTime::currentDateTime().toUTC().toString(Qt::ISODate);
   qDebug() << "Sending results at" << formattedDateTime;

   QNetworkAccessManager *nam = new QNetworkAccessManager();
   QNetworkRequest req;
   req.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/x-www-form-urlencoded"));
   req.setUrl(QUrl(_db_url));

   foreach (UPnPHash resultHash, results)
   {
       QHashIterator<extendedUPnP::DataType, QVariant> iter(resultHash);

       // results from one interface
       QVariantMap deviceResult;
       while (iter.hasNext())
       {
           iter.next();
           QString name = enumToString(extendedUPnP, DataType, iter.key());
           name = name.replace(QRegExp("([A-Z])"), "_\\1").toLower();
           name.remove(0, 1);

           deviceResult.insert(name, iter.value());
       }
       if(!deviceResult.contains("found_content"))
       {
           deviceResult.insert("found_content", QVariant());
       }
       QUrl rootUrl = QUrl(resultHash.value(RootDescUrl).toString());
       QString ipFull = rootUrl.host();
       ipFull.append(':');
       QString p;
       p.setNum(rootUrl.port());
       ipFull.append(p);
       deviceResult.insert("ip", ipFull);
       deviceResult.insert("post_date", formattedDateTime);
       QJsonObject obj = QJsonObject::fromVariantMap(deviceResult);
       QJsonDocument doc(obj);
       QString jsonString = doc.toJson(QJsonDocument::Compact);
       QByteArray data;
       data.append(jsonString);
       _rep = nam->post(req, data);
   }
   if(results.isEmpty())
   {
       QVariantMap deviceResult;
       deviceResult.insert("post_date", formattedDateTime);
       QByteArray data((QJsonDocument
                        (QJsonObject::fromVariantMap(deviceResult)))
                       .toJson(QJsonDocument::Compact));
       qDebug() << data;
       _rep = nam->post(req, data);
   }
   connect(_rep, SIGNAL(readyRead()), this, SLOT(readAnswer()));
}

QList<extendedUPnP::UPnPHash> extendedUPnP::quickDevicesCheck(UPNPDev *list)
{
    QList<UPnPHash> myResults;

    for (UPNPDev *l = list; l; l = l->pNext)
    {
        UPNPUrls urls;
        IGDdatas data;
        char lanaddr[64];
        UPnPHash resultHash;
        qDebug() << "Checking " << l->descURL;
        int xmlFound = UPNP_GetIGDFromUrl(l->descURL, &urls, &data, lanaddr, sizeof(lanaddr));
        if(xmlFound)
        {
            /* These URLs will be needed for accessing and controlling Mediaservers with SOAP */
            QString controlURL = data.tmp.controlurl;
            if(!controlURL.isEmpty())
            {
                resultHash.insert(ControlUrl, controlURL);
            }

            QString eventSubUrl = data.tmp.eventsuburl;
            if(!eventSubUrl.isEmpty())
            {
                resultHash.insert(EventSubUrl, eventSubUrl);
            }

            QString serviceType = data.tmp.servicetype;
            if(!serviceType.isEmpty())
            {
                resultHash.insert(ServiceType, serviceType);
            }

            QString rootDescURL = urls.rootdescURL;
            if(!rootDescURL.isEmpty())
            {
                resultHash.insert(RootDescUrl, rootDescURL);
            }
            int bufferSize = 0;
            if (char *buffer = (char *)miniwget(urls.rootdescURL, &bufferSize, 0))
            {
                NameValueParserData pdata;
                ParseNameValue(buffer, bufferSize, &pdata);
                free(buffer);
                QStringList modelName = GetValsFromNameValueList(&pdata, "modelName");

                if (!modelName.isEmpty())
                {
                    resultHash.insert(ModelName, modelName.last());
                }

                QStringList manufacturer = GetValsFromNameValueList(&pdata, "manufacturer");

                if (!manufacturer.isEmpty())
                {
                    resultHash.insert(Manufacturer, manufacturer.last());
                }

                QStringList friendlyName = GetValsFromNameValueList(&pdata, "friendlyName");

                if (!friendlyName.isEmpty())
                {
                    resultHash.insert(FriendlyName, friendlyName.last());
                }
                qDebug() << friendlyName << modelName << manufacturer;

                ClearNameValueList(&pdata);
            }
            FreeUPNPUrls(&urls);
            myResults.append(resultHash);
        }
    }
    freeUPNPDevlist(list);
    return myResults;
}

void extendedUPnP::printResultsToMap(QVariantList *list)
{
    QMap<QString, QString> m;

    foreach (m, m_handler->totalTableOfContents()) {
        QVariantMap entry;
        foreach(QString key, m.keys())
        {
            entry.insertMulti(key, m.value(key));
        }
        list->append(entry);
    }
}

void extendedUPnP::readAnswer()
{
    QByteArray ba = _rep->readAll();
    if(ba.contains("\"created\":true"))
    {
        qDebug() << "Successful: reply from server: " << ba;
        delete m_handler;
        QCoreApplication::exit(0);
    }else if(ba.contains("\"created\":false"))
    {
        qDebug() << "Unsuccessful: reply from server: " << ba;
        delete m_handler;
        QCoreApplication::exit(-1);
    }
}

