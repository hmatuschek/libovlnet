#include <QCoreApplication>
#include "lib/upnp.hh"
#include "lib/logger.hh"


int main(int argc, char *argv[]) {
  Logger::addHandler(new IOLogHandler(LogMessage::DEBUG));

  QCoreApplication app(argc, argv);

  UPNP upnp;
  upnp.connect(&upnp, SIGNAL(foundUPnPDevice(QUrl)), &upnp, SLOT(getDescription(QUrl)));
  upnp.discover();

  app.exec();
}
