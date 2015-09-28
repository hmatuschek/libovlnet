#ifndef SECURECHAT_H
#define SECURECHAT_H

#include "lib/crypto.h"
#include <QObject>

class Application;


class SecureChat : public QObject, public SecureStream
{
  Q_OBJECT

public:
  SecureChat(Application &application, QObject *parent=0);
  virtual ~SecureChat();

  void handleDatagram(uint32_t seq, const uint8_t *data, size_t len);

public slots:
  void sendMessage(const QString &msg);

signals:
  void messageReceived(const QString &msg);

protected:
  Application &_application;
};

#endif // SECURECHAT_H
