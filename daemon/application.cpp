#include "application.h"
#include "echostream.h"

#include <QDir>
#include <QFile>

Application::Application(int argc, char *argv[]) :
  QCoreApplication(argc, argv)
{
  // Try to load identity from file
  QDir vlfDir = QDir::home();
  if (! vlfDir.cd(".vlf")) { vlfDir.mkdir(".vlf"); vlfDir.cd(".vlf"); }
  QFile idFile(vlfDir.canonicalPath()+"/identity.pem");
  if (!idFile.exists()) {
    qDebug() << "No identity found -> create one.";
    _identity = Identity::newIdentity(idFile);
  } else {
    qDebug() << "Load identity from" << idFile.fileName();
    _identity = Identity::load(idFile);
  }

  if (_identity) {
    _dht = new DHT(_identity->id());
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