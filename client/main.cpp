#include "lib/dht.h"
#include <QApplication>
#include <time.h>

int main(int argc, char *argv[]) {
  qsrand(time(0));

  QApplication app(argc, argv);

  Node a(Identifier(), QHostAddress::LocalHost, 7740);
  Node b(Identifier(), QHostAddress::LocalHost, 7741);
  Node c(Identifier(), QHostAddress::LocalHost, 7742);

  b.ping(QHostAddress::LocalHost, 7740);
  c.ping(QHostAddress::LocalHost, 7740);

  app.exec();

  return 0;
}
