#include "lib/node.hh"
#include "lib/httpproxy.hh"
#include <iostream>

#include <QCoreApplication>

int main(int argc, char *argv[])
{
  Logger::addHandler(new IOLogHandler());

  QCoreApplication app(argc, argv);

  if (argc<2) {
    std::cout << "Usage: ovlhttpproxy BOOSTRAP_NODE [PROXY_PORT]" << std::endl;
    return -1;
  }

  uint16_t port = 8080;
  if (argc>2) {
    port = atoi(argv[2]);
  }

  Identity *id = Identity::newIdentity();
  Node dht(*id);
  LocalHttpProxyServer proxy(dht, port);

  // Bootstrap
  dht.ping(argv[1], 7741);
  dht.ping(argv[1], 7742);

  // run...
  app.exec();
}
