#include <QCoreApplication>
#include "lib/upnp.hh"
#include "lib/logger.hh"


int main(int argc, char *argv[]) {
  Logger::addHandler(new IOLogHandler(LogMessage::DEBUG));

  QCoreApplication app(argc, argv);

  UPNP upnp(7741, 7741);
  upnp.discover();

  app.exec();
}
