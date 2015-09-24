#include "lib/dht.h"
#include "lib/ntp.h"
#include "lib/pcp.h"

#include <QApplication>
#include <QDebug>

#include <time.h>

int main(int argc, char *argv[]) {
  qsrand(time(0));

  QApplication app(argc, argv);

  /*DHT node(Identifier(), QHostAddress::Any, 7741);
  node.ping("pc49-20.psych.uni-potsdam.de", 7741); */

  PCPClient pcp;
  pcp.requestMap(7741, QHostAddress("192.168.1.1"));
  app.exec();

  return 0;
}
