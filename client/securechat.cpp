#include "securechat.h"
#include "application.h"


SecureChat::SecureChat(Application &application)
  : QObject(0), SecureSocket(application.dht()),
    _application(application), _keepAlive(), _timeout()
{
  // Setup keep-alive timer
  _keepAlive.setInterval(1000*10);
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
SecureChat::started() {
  // start timers
  _keepAlive.start();
  _timeout.start();
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




