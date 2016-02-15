/** @defgroup chat Secure chat service
 * @ingroup services */

#ifndef SECURECHAT_H
#define SECURECHAT_H

#include "lib/crypto.h"

#include <QObject>
#include <QTimer>

/** Implements a trivial chat message connection.
 * This connection is not reliable, meaning it is not ensured that chat message will reach its
 * destination.
 * @ingroup chat */
class SecureChat : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  /** Construtor.
   * @param dht Specifies the @c DHT instance. */
  SecureChat(DHT &dht);
  /** Destructor, closes the connection. */
  virtual ~SecureChat();

public slots:
  /** Sets a message to the remote. */
  void sendMessage(const QString &msg);

signals:
  /** Gets emitted once the connection is established. */
  void started();
  /** Gets emitted if a message was received. */
  void messageReceived(const QString &msg);
  /** Gets emitted if the connection to the remote is closed or got lost. */
  void closed();

protected:
  /** Initializes the chat once the connection is established. */
  bool start(const Identifier &streamId, const PeerItem &peer);
  /** Gets called if the connection cannot be established. */
  void failed();
  /** Implemetes the SecureSocket interface. */
  void handleDatagram(const uint8_t *data, size_t len);

protected slots:
  /** Gets called by the @c _keepAlive timer to keep the connection alive. */
  void _onKeepAlive();
  /** Gets called on a connection timeout. */
  void _onTimeout();

protected:
  /** Periodically sends a null message to the remote to keep the connection alive. */
  QTimer _keepAlive;
  /** This timer will be restarted everytime a message (incl. a null message) is received. This
   * can be used as a connection timeout. */
  QTimer _timeout;
};

#endif // SECURECHAT_H
