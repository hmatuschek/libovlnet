#include "securechat.h"
#include "application.h"


SecureChat::SecureChat(Application &application, QObject *parent)
  : QObject(parent), SecureStream(application.identity()), _application(application)
{
  // pass...
}

SecureChat::~SecureChat() {
  _application.dht().closeStream(_streamId);
}

void
SecureChat::handleDatagram(uint32_t seq, const uint8_t *data, size_t len) {
  qDebug() << "Received datagram" << seq << ": " << QByteArray((const char *)data, len);
  emit messageReceived(
        QString::fromUtf8(QByteArray((const char *)data, len)));
}

void
SecureChat::sendMessage(const QString &msg) {
  QByteArray data = msg.toUtf8();
  this->sendDatagram((uint8_t *)data.data(), data.size());
}




