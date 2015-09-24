#include "lib/dht.h"
#include "lib/ntp.h"

#include <QApplication>
#include <QDebug>

#include <time.h>

int main(int argc, char *argv[]) {
  qsrand(time(0));

  QApplication app(argc, argv);

  DHT node1(Identifier(), QHostAddress::LocalHost, 7741);
  DHT node2(Identifier(), QHostAddress::LocalHost, 7742);

  node2.ping(QHostAddress::LocalHost, 7741);

  app.exec();

  return 0;
}
