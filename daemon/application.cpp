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
  if (! vlfDir.cd("ovlnetd")) {
    vlfDir.mkdir("ovlnetd"); vlfDir.cd("ovlnetd");
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
    _dht = new DHT(*_identity);
  } else {
    logError() << "Error while loading or creating my identity.";
  }

  // Register services
  _dht->registerService(2, new HalChatService(*this));
  _dht->registerService(5, new SocksService(*this));
}

DHT &
Application::dht() {
  return *_dht;
}


/* ********************************************************************************************* *
 * Implementation of HalChatService
 * ********************************************************************************************* */
Application::HalChatService::HalChatService(Application &app)
  : AbstractService(), _application(app)
{
  // pass...
}

SecureSocket *
Application::HalChatService::newSocket() {
  return new HalChat(_application.dht(), _application._model);
}

bool
Application::HalChatService::allowConnection(const NodeItem &peer) {
  return true;
}

void
Application::HalChatService::connectionStarted(SecureSocket *stream) {
  static_cast<HalChat *>(stream)->started();
}

void
Application::HalChatService::connectionFailed(SecureSocket *stream) {
  delete stream;
}


/* ********************************************************************************************* *
 * Implementation of SocksService
 * ********************************************************************************************* */
Application::SocksService::SocksService(Application &app)
  : AbstractService(), _application(app)
{
  // pass...
}

SecureSocket *
Application::SocksService::newSocket() {
  return new SocksOutStream(_application.dht());
}

bool
Application::SocksService::allowConnection(const NodeItem &peer) {
  return _application._settings.socksServiceWhiteList().contains(peer.id());
}

void
Application::SocksService::connectionStarted(SecureSocket *stream) {
  dynamic_cast<SocksOutStream *>(stream)->open(QIODevice::ReadWrite);
}

void
Application::SocksService::connectionFailed(SecureSocket *stream) {
  delete stream;
}
