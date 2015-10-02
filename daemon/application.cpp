#include "application.h"
#include "echostream.h"

#include <QDir>
#include <QFile>

Application::Application(int argc, char *argv[]) :
  QCoreApplication(argc, argv)
{
  // Try to load identity from file
  QDir vlfDir("/etc");
  // if deamon directory does not exists -> create it
  if (! vlfDir.cd("vlfdaemon")) {
    vlfDir.mkdir("vlfdaemon"); vlfDir.cd("vlfdaemon");
  }
  // Create identity if not present
  QString idFile(vlfDir.canonicalPath()+"/identity.pem");
  if (!QFile::exists(idFile)) {
    qDebug() << "No identity found -> create one.";
    _identity = Identity::newIdentity(idFile);
  } else {
    qDebug() << "Load identity from" << idFile;
    _identity = Identity::load(idFile);
  }

  if (_identity) {
    _dht = new DHT(_identity->id(), this);
  } else {
    qDebug() << "Error while loading or creating my identity.";
  }
}

SecureStream *
Application::newStream(uint16_t service) {
  return new EchoStream(*_identity);
}

bool
Application::allowStream(uint16_t service, const NodeItem &peer) {
  return true;
}

void
Application::streamStarted(SecureStream *stream) {
  qDebug() << "Stream service" << stream << "started";
}
