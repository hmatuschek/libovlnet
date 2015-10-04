#ifndef SECURECHAT_H
#define SECURECHAT_H

#include "lib/crypto.h"
#include <QObject>

class Application;


class SecureChat : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  SecureChat(Application &application);
  virtual ~SecureChat();

  void handleDatagram(const uint8_t *data, size_t len);

public slots:
  void sendMessage(const QString &msg);
  void keepAlive();

signals:
  void messageReceived(const QString &msg);

protected slots:
  void _onKeepAlive();

protected:
  Application &_application;
  QTimer _keepAlive;
};

#endif // SECURECHAT_H
