#include "halchat.hh"

HalChat::HalChat(Node &dht, QHalModel &model, QObject *parent)
  : QObject(parent), SecureSocket(dht), _model(model), _keepAlive(), _timeout()
{
  // Setup "keep alive" timer
  _keepAlive.setInterval(5000);
  _keepAlive.setSingleShot(false);

  // Setup "timeout" timer
  _timeout.setInterval(1000*60);
  _timeout.setSingleShot(true);

  connect(&_keepAlive, SIGNAL(timeout()), this, SLOT(_onKeepAlive()));
  connect(&_timeout, SIGNAL(timeout()), this, SLOT(_onTimeout()));
}

void
HalChat::started() {
  _keepAlive.start();
  _timeout.start();
}

void
HalChat::handleDatagram(const uint8_t *data, size_t len) {
  // Restart timeout timer if a packet arives
  _timeout.start();
  // Ingnore null datagrams
  if ((0 == data) && (0 == len)) { return; }
  // Handle messages
  QString msg = QString::fromUtf8((const char *)data, len);
  logDebug() << "Got '" << msg << "'.";
  msg = _model.reply(msg);
  logDebug() << "Reply '" << msg << "'.";
  QByteArray buffer = msg.toUtf8();
  // Send response
  sendDatagram((const uint8_t *)buffer.constData(), buffer.size());
}

void
HalChat::_onKeepAlive() {
  sendNull();
}

void
HalChat::_onTimeout() {
  _keepAlive.stop();
  this->deleteLater();
}
