#ifndef SECURECHAT_H
#define SECURECHAT_H

#include "lib/crypto.h"

#include <QObject>
#include <QTimer>

class Application;

/** Implements a trivial chat message service. */
class SecureChat : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  SecureChat(Application &application);
  virtual ~SecureChat();

public slots:
  void started();
  void sendMessage(const QString &msg);

signals:
  void messageReceived(const QString &msg);
  void closed();

protected:
  void handleDatagram(const uint8_t *data, size_t len);

protected slots:
  void _onKeepAlive();
  void _onTimeout();

protected:
  Application &_application;
  QTimer _keepAlive;
  QTimer _timeout;
};

#endif // SECURECHAT_H
