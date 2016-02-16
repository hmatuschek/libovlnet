#include "application.hh"
#include "echostream.hh"
#include "halchat.hh"
#include "lib/socks.h"
#include "lib/secureshell.h"
#include "httpstatus.hh"

#include <QDir>
#include <QFile>

Application::Application(int argc, char *argv[])
  : QCoreApplication(argc, argv), _model(), _settings("/etc/ovlnetd/settings.json")
{
  // Try to load identity from file
  QDir ovlDir("/etc");
  // if deamon directory does not exists -> create it
  if (! ovlDir.cd("ovlnetd")) {
    ovlDir.mkdir("ovlnetd"); ovlDir.cd("ovlnetd");
  }
  // Create identity if not present
  QString idFile(ovlDir.canonicalPath()+"/identity.pem");
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
  _dht->registerService(80, new HttpService(*this->_dht, new HttpStatus(*this->_dht)));
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
  logDebug() << "ChatService: Allow connection to " << peer.id()
             << " @" << peer.addr() << ".";
  return true;
}

void
Application::HalChatService::connectionStarted(SecureSocket *stream) {
  logDebug() << "ChatService: Connection to " << stream->peerId() << " started.";
  static_cast<HalChat *>(stream)->started();
}

void
Application::HalChatService::connectionFailed(SecureSocket *stream) {
  logDebug() << "ChatService: Connection to " << stream->peerId() << " failed.";
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
  if (_application._settings.socksServiceWhiteList().contains(peer.id())) {
    logDebug() << "SocksService: Allow SOCKS connection from " << peer.id() << ".";
  } else {
    logDebug() << "SocksService: Deny SOCKS connection from " << peer.id() << ".";
  }
  return _application._settings.socksServiceWhiteList().contains(peer.id());
}

void
Application::SocksService::connectionStarted(SecureSocket *stream) {
  logDebug() << "SocksService: SOCKS connection to " << stream->peerId() << " started.";
  dynamic_cast<SocksOutStream *>(stream)->open(QIODevice::ReadWrite);
}

void
Application::SocksService::connectionFailed(SecureSocket *stream) {
  logDebug() << "SocksService: SOCKS connection to " << stream->peerId() << " failed.";
  delete stream;
}
