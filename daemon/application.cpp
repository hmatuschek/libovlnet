#include "application.h"
#include "echostream.h"
#include "halchat.h"
#include "lib/socks.h"
#include "lib/secureshell.h"

#include <QDir>
#include <QFile>

Application::Application(int argc, char *argv[])
  : QCoreApplication(argc, argv), _model(), _settings("/etc/ovlnetd/settings.json")
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
    logInfo() << "No identity found -> create one.";
    _identity = Identity::newIdentity();
    if (_identity) { _identity->save(idFile); }
  } else {
    logDebug() << "Load identity from" << idFile;
    _identity = Identity::load(idFile);
  }

  if (_identity) {
    _dht = new DHT(*_identity, this);
  } else {
    logError() << "Error while loading or creating my identity.";
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
    if (_settings.socksServiceWhiteList().isEmpty()) {
      logInfo() << "No whitelisted nodes for SOCKS service -> deny.";
      return 0;
    }
    // create handler
    return new SocksOutStream(*_dht);
  } else if (22 == service) {
    if (_settings.shellServiceWhiteList().isEmpty()) {
      logInfo() << "No whitelisted nodes for shell service -> deny.";
      return 0;
    }
    // create handler
    return new SecureShell(*_dht);
  }
  return 0;
}

bool
Application::allowConnection(uint16_t service, const NodeItem &peer) {
  if (2 == service) {
    // HalChat is public
    return true;
  } else if (5 == service) {
    if (_settings.socksServiceWhiteList().contains(peer.id())) {
      logDebug() << "Allow SOCKS connection from " << peer.id()
                 << " @" << peer.addr() << ":" << peer.port();
    } else {
      logInfo() << "Deny SOCKS connection from " << peer.id()
                << " @" << peer.addr() << ":" << peer.port();
    }
    // Check if node is allowed to use the SOCKS service
    return _settings.socksServiceWhiteList().contains(peer.id());
  } else if (22 == service) {
    if (_settings.shellServiceWhiteList().contains(peer.id())) {
      logDebug() << "Allow shell connection from " << peer.id()
                 << " @" << peer.addr() << ":" << peer.port();
    } else {
      logInfo() << "Deny shell connection from " << peer.id()
                << " @" << peer.addr() << ":" << peer.port();
    }
    // Check if node is allowed to use the shell service
    return _settings.shellServiceWhiteList().contains(peer.id());
  }
  return true;
}

void
Application::connectionStarted(SecureSocket *stream) {
  logDebug() << "Stream service " << stream << " started";
  if (HalChat *chat = dynamic_cast<HalChat *>(stream)) {
    // start chat (keep alive messages etc. )
    chat->started();
  } else if (SocksOutStream *socks = dynamic_cast<SocksOutStream *>(stream)) {
    // start SOCKS service.
    socks->open(QIODevice::ReadWrite);
  } else if (SecureShell *shell = dynamic_cast<SecureShell *>(stream)) {
    // start shell
    shell->open(QIODevice::ReadWrite);
  }
}

void
Application::connectionFailed(SecureSocket *stream) {
  // mhh, don't care.
}
