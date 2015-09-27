#ifndef SECURECHAT_H
#define SECURECHAT_H

#include "lib/crypto.h"
#include <QObject>

class SecureChat : public QObject, public SecureStream
{
  Q_OBJECT

public:
  SecureChat(Identity &id, QObject *parent=0);

  void handleDatagram(uint32_t seq, const uint8_t *data, size_t len);

public slots:
  void sendMessage(const QString &msg);

signals:
  void messageReceived(const QString &msg);
};

#endif // SECURECHAT_H
