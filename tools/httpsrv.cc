#include "lib/ovlnet.hh"
#include <QCoreApplication>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc<3) {
    std::cout << "USAGE: ovlhttpsrv DIRECTORY BOOTSTRAP [PORT]";
    return -1;
  }

  uint16_t port = 7741;
  if (argc==4) { port = atoi(argv[3]); }

  Logger::addHandler(new IOLogHandler());

  QCoreApplication app(argc, argv);

  Identity *id = Identity::newIdentity();
  DHT node(*id, QHostAddress::Any, port);
  HttpDirectoryHandler *handler = new HttpDirectoryHandler(QDir(argv[1]));
  node.registerService(80, new HttpService(node, handler));

  node.ping(argv[2], 7741);
  node.ping(argv[2], 7742);

  app.exec();
}
