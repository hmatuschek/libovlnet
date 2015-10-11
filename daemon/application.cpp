#include "application.h"
#include "echostream.h"
#include "halchat.h"
#include "lib/socks.h"
#include "socksconnection.h"

#include <QDir>
#include <QFile>

Application::Application(int argc, char *argv[])
  : QCoreApplication(argc, argv), _model(), _socksWhiteList("/etc/vlfdaemon/sockswhitelist.json")
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
    _dht = new DHT(*_identity, this);
  } else {
    qDebug() << "Error while loading or creating my identity.";
  }
}

DHT &
Application::dht() {
  return *_dht;
}

SecureSocket *
Application::newSocket(uint16_t service) {
  if (2 == service) {
    // neat public chat service
    return new HalChat(*_dht, _model);
  } else if (5 == service) {
    if (_socksWhiteList.empty()) { return 0; }
    // create handler
    return new SOCKSConnection(*this);
  }
  return 0;
}

bool
Application::allowConnection(uint16_t service, const NodeItem &peer) {
  if (2 == service) {
    // HalChat is public
    return true;
  } else if (5 == service) {
    if (_socksWhiteList.allowed(peer.id())) {
      qDebug() << "Allow SOCKS connection from" << peer.id()
               << peer.addr() << ":" << peer.port();
    } else {
      qDebug() << "Deny SOCKS connection from" << peer.id()
               << peer.addr() << ":" << peer.port();
    }
    // Check if node is allowed to use the SOCKS service
    return _socksWhiteList.allowed(peer.id());
  }
  return true;
}

void
Application::connectionStarted(SecureSocket *stream) {
  qDebug() << "Stream service" << stream << "started";
  HalChat *chat = 0;
  SOCKSConnection *socks = 0;
  if (0 != (chat = dynamic_cast<HalChat *>(stream))) {
    // start chat (keep alive messages etc. )
    chat->started();
  } else if (0 != (socks = dynamic_cast<SOCKSConnection *>(stream))) {
    // start SOCKS service.
    socks->open(QIODevice::ReadWrite);
  }
}

void
Application::connectionFailed(SecureSocket *stream) {
  // mhh, don't care.
}
