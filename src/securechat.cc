#include "securechat.hh"
#include "node.hh"

SecureChat::SecureChat(Network &node)
  : QObject(0), SecureSocket(node),
    _keepAlive(), _timeout()
{
  // Setup keep-alive timer
  _keepAlive.setInterval(1000*5);
  _keepAlive.setSingleShot(false);
  // Setup watchdog timer
  _timeout.setInterval(1000*60);
  _timeout.setSingleShot(true);

  connect(&_keepAlive, SIGNAL(timeout()), this, SLOT(_onKeepAlive()));
  connect(&_timeout, SIGNAL(timeout()), this, SLOT(_onTimeout()));
}

SecureChat::~SecureChat() {
  // pass...
}

bool
SecureChat::start(const Identifier &streamId, const PeerItem &peer) {
  if (SecureSocket::start(streamId, peer)) {
    logDebug() << "SecureChat: Connection to " << peer.addr() << " started.";
    // start timers
    _keepAlive.start();
    _timeout.start();
    // signal chat started
    emit started();
    // done.
    return true;
  }
  logDebug() << "SecureChat: Connection to " << peer.addr() << " failed.";
  return false;
}

void
SecureChat::failed() {
  emit closed();
}

void
SecureChat::handleDatagram(const uint8_t *data, size_t len) {
  // On any message -> reset timeout
  _timeout.start();
  // Ignore null datagrams
  if ((0 == len) && (0 == data)) { return; }
  // Handle messages
  emit messageReceived(
        QString::fromUtf8(QByteArray((const char *)data, len)));
}

void
SecureChat::sendMessage(const QString &msg) {
  QByteArray data = msg.toUtf8();
  sendDatagram((uint8_t *)data.data(), data.size());
}

void
SecureChat::_onKeepAlive() {
  // Send an empty datagram
  sendNull();
}

void
SecureChat::_onTimeout() {
  // On timeout -> stop keep-alive pings
  _keepAlive.stop();
  // and signal connection loss
  emit closed();
}




