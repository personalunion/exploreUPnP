#include "UPnPHandler.h"
#include <QHttpPart>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkConfiguration>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QTimer>
#include <QtAlgorithms>

#include <QXmlStreamWriter>
#include <QThread>
#include <iterator>
#include <QTcpServer>
#include "../miniupnp/miniupnpc/miniwget.h"

UPnPHandler::UPnPHandler()
{
}


UPnPHandler::~UPnPHandler()
{

}

int UPnPHandler::init(QUrl descriptionUrl, QString eventSubUrl, QString controlUrl, QString serviceType)
{
    QList<QUrl> ownUrls;
    foreach (const QHostAddress &address, QNetworkInterface::allAddresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost))
        {
            QString s = QString("http://%1").arg(address.toString());
            ownUrls.append(QUrl(s));
        }
    }

    QString s = descriptionUrl.url().remove(descriptionUrl.path());
    setRemoteUrl(QUrl(s));
    setOwnUrls(ownUrls);

    QUrl subscribeUrl, browseUrl;
    QUrl descUrl(descriptionUrl);
    browseUrl = subscribeUrl = m_remoteUrl;
    browseUrl.setPath(controlUrl);
    subscribeUrl.setPath(eventSubUrl);

    m_GETUrl = descUrl;
    setActionUrl(browseUrl);
    setSubscribeUrl(subscribeUrl);
    setServicetype(serviceType);
    setFile(":/xml/soapData.xml");
    /* If the file is already open, do nothing */
    if(!m_file->isOpen())
    {
        if (!m_file->open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Opening XML File Problem";
            return -1;
        }
    }
    m_soapData = m_file->readAll();
    m_file->close();
    m_answerFromServer = "";

    m_parser = new Parser();
    /* will be changed later, after all levels have been gone trough */
    m_parser->setSearchTerm("container");
    int ret = startGet();
    //m_socket->close();
    delete m_parser;
    return ret;
}

int UPnPHandler::cleanup()
{
    m_totalTableOfContents.clear();
    return 0;
}

int UPnPHandler::getReadyRead(QByteArray content)
{
    int end1 = content.indexOf("400 Bad Request");
    if(end1 != -1)
    {
        qDebug() << "400 Bad Request";
        return -3;
    }
    int end = content.indexOf("<?xml version=\"1.0\"");
    if(end == -1)
    {
        qDebug() << "No xml data was found";
        return -2;
    }

    content.remove(0, end);
    QString serviceType = "urn:schemas-upnp-org:service:ContentDirectory:1";
    QList<QMap<QString, QString> > *urls = new QList<QMap<QString, QString> >();
    int ret = m_parser->parseRootXML(content, urls, serviceType);
    if(ret != -1)
    {
        /* Making sure no useless new urls are inserted */
        if(!urls->isEmpty() &&
           !urls->at(0).value("controlURL").isEmpty() &&
           !urls->at(0).value("eventSubURL").isEmpty())
        {
            QUrl subscribeUrl, browseUrl;
            browseUrl = subscribeUrl = m_remoteUrl;
            browseUrl.setPath(urls->at(0).value("controlURL"));
            subscribeUrl.setPath(urls->at(0).value("eventSubURL"));
            setActionUrl(browseUrl);
            setSubscribeUrl(subscribeUrl);
            setServicetype(serviceType);
        }
    }

    return ret;
}

int UPnPHandler::subscribe()
{
    /* Now subscribe */
    int ret = 0;
    int port = defaultPort;
    QTcpServer listener;
    listener.listen(QHostAddress::Any,port);
    QByteArray valTemplate = ("</>");
    QString val = valTemplate; //for loop
    QTcpSocket * socket = new QTcpSocket();
    socket->connectToHost(m_subscribeUrl.host(), m_subscribeUrl.port());
    QString url = m_subscribeUrl.host();
    /* There are other localhost addresses available in m_ownUrls
       but it does not seem to make a difference, which one is used
       TODO check, maybe with loop */
    QString s = m_ownUrls[0].host();
    s.prepend("http://");
    val.insert(1, m_ownUrls[0].toString());
    QString header = QString( "SUBSCRIBE %1 HTTP/1.1\r\n"
                              "HOST: %2:%3\r\n"
                              "CALLBACK: <%4:%5/>\r\n"
                              "NT: upnp:event\r\n"
                              "TIMEOUT:Second-1810\r\n\r\n").arg(m_subscribeUrl.path())
                                                            .arg(url).arg(m_subscribeUrl.port())
                                                            .arg(s).arg(port);
    int bytesWritten = 0;

    /* Send the SOAP request -> HTTP POST header and xml data */
    while(bytesWritten < header.length())
    {
        bytesWritten += socket->write(header.mid(bytesWritten).toLatin1());
        /* on error */
        if(bytesWritten < 0)
        {
            socket->close();
            delete socket;
            ret = -1;
        }
    }
    bytesWritten = 0;
    if (socket->waitForReadyRead(firstByteReceivedTimeout))
    {
        QByteArray ba = socket->readAll();
        socket->close();
        delete socket;
    }
    else
    {
        delete socket;
        ret = -1;
        //m_socket->close();
    }
    if(listener.waitForNewConnection(3000))
    {
        QTcpSocket * socket = listener.nextPendingConnection();
        QString s;
        if (socket->waitForReadyRead(3000))
        {
            s = socket->readAll();
            QString url = m_remoteUrl.toString();
            QByteArray ok = QByteArray("<html><body><h1>200 OK</h1></body></html>");
            int dataLength = ok.length();
            url.remove("http://");
            QString subHeader = QString("HTTP/1.1 200 OK\r\n"
                                        "SERVER: Linux/3.13.0-24-generic, UPnP/1.0, Portable SDK for UPnP devices/1.6.17\r\n"
                                        "CONNECTION: close\r\n"
                                        "CONTENT-LENGTH: %1\r\n"
                                        "CONTENT-TYPE: text/html\r\n\r\n").arg(dataLength);
            subHeader.append(ok);
            int bytesWritten = 0;

            while(bytesWritten < subHeader.length())
            {
                bytesWritten += socket->write(subHeader.mid(bytesWritten).toLatin1());
                /* on error */
                if(bytesWritten < 0)
                {
                    socket->close();
                    delete socket;
                }
            }
            if(socket->waitForReadyRead(3000))
            {
                QByteArray answer = socket->readAll();
            }
            bytesWritten = 0;
            socket->close();
            delete socket;
        }
        else
        {
            socket->close();
            delete socket;
        }
    }else{
        qDebug() << "No tcp connection established"; //<< listener.errorString(); //TODO check why always
    }
    listener.close();
    setupTCPSocketAndSend("0", 0);
    ret = (m_totalTableOfContents.length() != 0) ? 1 : -1;
    if(printResults() == 0)
    {
        ret = -1;
    }
    //delete socket;
    return ret;
}

//TODO timer for glimpse
QList<QPair<QString, QString> > UPnPHandler::sendRequest(QString objectID)
{
    /* different values for the object-IDs need to be inserted. starting with 0;
     * parsing the answer and then again send request to the corresponding object-IDs */
    QByteArray b;
    b.append(objectID);
    QByteArray data = m_soapData;
    data.replace("##wildcard##", QByteArray(b));
    int dataLength = data.length();
    QString url = m_remoteUrl.toString();
    url.remove("http://");
    if(url.right(1) == "/")
    {
        url.chop(1);
    }
    QString header = QString("POST %1 HTTP/1.1\r\n"
                              "HOST: %2\r\n"
                              "CONTENT-LENGTH: %3\r\n"
                              "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
                              "SOAPACTION: \"%4#Browse\"\r\n"
                              "USER-AGENT: Linux/3.13.0-24-generic, UPnP/1.0, Portable SDK for UPnP devices/1.6.17\r\n\r\n").arg(m_actionUrl.path())
                                                                                                                            .arg(url)
                                                                                                                            .arg(dataLength)
                                                                                                                            .arg(m_servicetype);
    header.append(data);
    int bytesWritten = 0;

    /* Send the SOAP request -> HTTP POST header and xml data */
    while(bytesWritten < header.length())
    {
        bytesWritten += m_socket->write(header.mid(bytesWritten).toLatin1());
        /* on error */
        if(bytesWritten < 0)
        {
            m_socket->close();
        }
    }
    bytesWritten = 0;
    QList<QPair<QString, QString> > containers;
    if (m_socket->waitForReadyRead(firstByteReceivedTimeout))
    {
        try{
            containers = read();
        }catch(int e)
        {
            throw e;
        }
    }
    else
    {
        m_socket->close();
    }
    return containers ;
}

int UPnPHandler::setupTCPSocketAndSend(QString objectID, int counter)
{
    QList<QPair<QString, QString> > foundObjs;
    int savedPos = counter;
    while(true)
    {
        if(!startTCPConnection())
        {
            qDebug() << "No TCP connection established";
            return -1;
        }
        /* At first foundObjs list is empty,
         * later those lists are needed to step through
         * all containers*/
        if(foundObjs.isEmpty())
        {
            try{
                foundObjs = sendRequest(objectID);
            }catch(int e)
            {
                /* Since the package was not successfully received from server,
                 * a new TCP Connection has to be established -> loop*/
                m_socket->close();
                delete m_socket;
                continue;
            }
        }
        /* Handling items if available */
        if(!foundObjs.isEmpty() && counter < foundObjs.length())
        {
            m_socket->close();
            delete m_socket;
            counter = setupTCPSocketAndSend(foundObjs.at(counter).second, counter);
            continue;
        }
        else{
            counter++;
            break;
        }
        if(m_socket != NULL)
        {
            m_socket->close();
            delete m_socket;
        }
    }
    counter = savedPos + 1;
    /* If the level is finished,
     * the searchTerm must be set back to "container"
     * to find containers again */
    m_parser->setSearchTerm("container");
    m_socket->close();
    delete m_socket;
    return counter;
}

QList<QPair<QString, QString> > UPnPHandler::read()
{
    bytesReceived << m_socket->bytesAvailable();
    QByteArray ba;
    QByteArray line;
    while(!m_socket->atEnd())
    {
        line = m_socket->readLine(10000);
        ba.append(line);
        int i;
        QString cont = "CONTENT-LENGTH:";
        if((i = line.indexOf(cont))!=-1)
        {
            line.remove(i, cont.length());
            line = line.trimmed();
            m_expectedLength = line.toInt();
        }
    }
    /* A byteReceived count below 400 means,
     * that there was just the header sent,
     * so the package is damaged and
     * has to be requested again */
    if(bytesReceived.last() < 400)
    {
        throw 400;
    }
    m_answerFromServer = ba;
    m_parser->setRawData(m_answerFromServer);
    QList<QMap<QString, QString> > l;
    QList<QPair<QString, QString> > containers;
    /* To be sure if a http 200 packet came back and it has a xml part */
    if(m_answerFromServer.contains(QByteArray("200 OK")))
    {
        try{
            l = m_parser->parseUpnpReply(m_expectedLength);
        }catch(int e)
        {
            throw e;
        }
    }else
    {
        qDebug() << "Error from Server or incomplete package:\n---\n" << m_answerFromServer << "\n---\n";
        return containers;
    }
    if(l.length() != 0)
    {
        m_foundContent = l;
        containers = handleContent(m_parser->searchTerm());
        m_answerFromServer.clear(); //just deleting it in case of something else coming, otherwise no more data
    }else{
        //TODO
        /* No more container were found, so now the search term changes to 'item'
         * and search again
         */
        m_parser->setSearchTerm("item");
        m_parser->setRawData(m_answerFromServer);
        try{
            l = m_parser->parseUpnpReply(m_expectedLength);
        }catch(int e)
        {
            throw e;
        }
        m_foundContent = l;
        containers = handleContent(m_parser->searchTerm());
    }
    ba.clear();
    return containers;
}

int UPnPHandler::printResults()
{
    qDebug() << "A total of" << m_totalTableOfContents.length() << "elements was found";
    return m_totalTableOfContents.length();
}

void UPnPHandler::readSID()
{
    if(m_subscribeReply->error() != QNetworkReply::NoError)
    {
        //qDebug() << "Error occured while subscribe-request:" , m_subscribeReply->errorString();
        return;
    }
    QByteArray a = m_subscribeReply->readAll();
    //qDebug() << a;
}

QList<QMap<QString, QString> > UPnPHandler::totalTableOfContents() const
{
    return m_totalTableOfContents;
}

bool UPnPHandler::startTCPConnection()
{
    m_socket = new QTcpSocket();
    m_socket->connectToHost(m_actionUrl.host(), m_actionUrl.port());

    if (m_socket->waitForConnected(tcpConnectTimeout))
    {
        return true;
    }
    else
    {
        m_socket->close();
        return false;
    }
}

/**
 * @brief UPnPHandler::handleContent
 * @param t the string which was searched for in the parser before
 * @return number of elements found
 */

QList<QPair<QString, QString> > UPnPHandler::handleContent(QString t)
{
    QList<QPair<QString, QString> > objectIDsandPurpose;
    if(t == "container")
    {
        for(int i = 0; i < m_foundContent.length(); i++)
        {
            QString id = m_foundContent[i].value("id");
            QString title = m_foundContent[i].value("title");
            QPair<QString, QString> m;
            m.first = title;
            m.second = id;
            objectIDsandPurpose.append(m);
        }
    }else if (t == "item") {
        /* Copying only non-existant values into totalTableOfContents */
        for(int i = 0; i < m_foundContent.length(); i++)
        {
            int x = m_totalTableOfContents.indexOf(m_foundContent[i]);
            if(m_totalTableOfContents.indexOf(m_foundContent[i]) == -1)
            {
                m_totalTableOfContents.append(m_foundContent[i]);
            }
            else{
                qDebug() << "double found:" << m_foundContent[i] << m_totalTableOfContents[x];
            }
        }
        /* appending nothing to objectIDsandPurpose -> returning empty list */
    }
    return objectIDsandPurpose;
}

void UPnPHandler::setOwnUrls(const QList<QUrl> &ownUrls)
{
    m_ownUrls = ownUrls;
}

QList<QMap<QString, QString> > UPnPHandler::foundContent() const
{
    return m_foundContent;
}

QString UPnPHandler::servicetype() const
{
    return m_servicetype;
}

void UPnPHandler::setServicetype(const QString &servicetype)
{
    m_servicetype = servicetype;
}

int UPnPHandler::startGet()
{
    int ret = 0;
    QString url = m_remoteUrl.toString();
    url.remove("http://");
    if(url.right(1) == "/")
    {
        url.chop(1);
    }
    int descXMLsize = 0;
    char lanaddr[64];
    QString s = m_GETUrl.toString();
    QByteArray ba = s.toLatin1();
    const char *bla = ba.data();
    char *descXML = (char*)miniwget_getaddr(bla, &descXMLsize,
                               lanaddr, sizeof(lanaddr), 0);
    QString si = QString(descXML);
    QString str = si.simplified();
    str = str.replace("  ", "");
    str = str.replace("> <", "><");
    getReadyRead(str.toLatin1());
    ret = subscribe();
    return ret;
}

QUrl UPnPHandler::remoteUrl() const
{
    return m_remoteUrl;
}

void UPnPHandler::setRemoteUrl(const QUrl &url)
{
    m_remoteUrl = url;
}

QUrl UPnPHandler::actionUrl() const
{
    return m_actionUrl;
}

void UPnPHandler::setActionUrl(const QUrl &browseUrl)
{
    m_actionUrl = browseUrl;
}

QUrl UPnPHandler::subscribeUrl() const
{
    return m_subscribeUrl;
}

void UPnPHandler::setSubscribeUrl(const QUrl &subscribeUrl)
{
    m_subscribeUrl = subscribeUrl;
}

void UPnPHandler::setFile(const QString filename)
{
    m_file = new QFile(filename);
}
