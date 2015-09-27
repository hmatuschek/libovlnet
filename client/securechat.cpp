#include "securechat.h"

SecureChat::SecureChat(Identity &id, QObject *parent)
  : QObject(parent), SecureStream(id)
{

}

void
SecureChat::handleDatagram(uint32_t seq, const uint8_t *data, size_t len) {
  qDebug() << "Received datagram" << seq << ": " << QByteArray((const char *)data, len);
  emit messageReceived(QByteArray((const char *)data, len));
}

void
SecureChat::sendMessage(const QString &msg) {
  QByteArray data = msg.toLocal8Bit();
  this->sendDatagram((uint8_t *)data.data(), data.size());
}




