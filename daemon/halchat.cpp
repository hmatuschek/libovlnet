#include "halchat.h"

HalChat::HalChat(DHT &dht, QHalModel &model, QObject *parent)
  : QObject(parent), SecureSocket(dht), _model(model)
{
  // pass...
}

void
HalChat::handleDatagram(const uint8_t *data, size_t len) {
  if ((0 == data) && (0 == len)) { return; }
  QString msg = QString::fromUtf8((const char *)data, len);
  msg = _model.reply(msg);
  QByteArray buffer = msg.toUtf8();
  sendDatagram((const uint8_t *)buffer.constData(), buffer.size());
}
