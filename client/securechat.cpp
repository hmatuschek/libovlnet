#include "securechat.h"
#include "application.h"


SecureChat::SecureChat(bool incomming, Application &application, QObject *parent)
  : QObject(parent), SecureStream(incomming, application.identity()),
    _application(application), _keepAlive()
{
  _keepAlive.setInterval(1000*10);
  _keepAlive.setSingleShot(false);
  QObject::connect(&_keepAlive, SIGNAL(timeout()), this, SLOT(_onKeepAlive()));
}

SecureChat::~SecureChat() {
  _application.dht().closeStream(_streamId);
}

void
SecureChat::handleDatagram(uint32_t seq, const uint8_t *data, size_t len) {
  if ((0 == len) && (0 == seq) && (0 == data)) {
    /// @todo Handle "keep-alive" packet.
  } else {
    qDebug() << "Received datagram" << seq << ": " << QByteArray((const char *)data, len);
    emit messageReceived(
          QString::fromUtf8(QByteArray((const char *)data, len)));
  }
}

void
SecureChat::sendMessage(const QString &msg) {
  QByteArray data = msg.toUtf8();
  this->sendDatagram((uint8_t *)data.data(), data.size());
}

void
SecureChat::keepAlive() {
  if (!_keepAlive.isActive()) {
    _keepAlive.start();
  }
}

void
SecureChat::_onKeepAlive() {
  // Send an empty datagram
  sendNull();
}




