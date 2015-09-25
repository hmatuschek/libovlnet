#include "lib/dht.h"
#include "lib/ntp.h"
#include "lib/identity.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>

#include <time.h>

int main(int argc, char *argv[]) {
  qsrand(time(0));

  QCoreApplication app(argc, argv);
  Identity *identity = 0;
  // Try to load identity from file
  QDir vlfDir = QDir::home();
  if (! vlfDir.cd(".vlf")) { vlfDir.mkdir(".vlf"); vlfDir.cd(".vlf"); }
  QFile idFile(vlfDir.canonicalPath()+"/identity.pem");
  if (!idFile.exists()) {
    qDebug() << "No identity found -> create one.";
    identity = Identity::newIdentity(idFile);
  } else {
    qDebug() << "Load identity from" << idFile.fileName();
    identity = Identity::load(idFile);
  }

  if (! identity) {
    qDebug() << "Error while loading or creating my identity -> quit.";
    return -1;
  }

  DHT node(identity->id(), QHostAddress::Any, 7741);

  app.exec();

  return 0;
}
