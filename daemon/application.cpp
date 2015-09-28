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
  if (0 == service) {
    return new EchoStream(*_identity);
  }
  return 0;
}

bool
Application::allowStream(uint16_t service, const NodeItem &peer) {
  if (0 == service) {
    // echo service is accessiable for everyone:
    return true;
  }
  // Deny
  return false;
}

void
Application::streamStarted(SecureStream *stream) {
  qDebug() << "Stream service" << stream << "started";
}
