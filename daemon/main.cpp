#include "lib/dht.h"
#include "lib/ntp.h"

#include <QCoreApplication>
#include <QDebug>

#include <time.h>

int main(int argc, char *argv[]) {
  qsrand(time(0));

  QCoreApplication app(argc, argv);

  DHT node(Identifier(), QHostAddress::Any, 7741);

  app.exec();

  return 0;
}
