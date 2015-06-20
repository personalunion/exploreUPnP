#include "extendedupnp.h"
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    extendedUPnP scan;
    scan.start();
    return a.exec();
}
