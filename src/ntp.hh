#ifndef __VLF_NTP_TIMESTAMP_H__
#define __VLF_NTP_TIMESTAMP_H__

#include <QObject>
#include <QUdpSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QString>
#include <QTimer>
#include <inttypes.h>


/** Implements a simple NTP client.
 * @ingroup utils */
class NTPClient: public QObject
{
  Q_OBJECT

public:
  /** Constructor. */
  explicit NTPClient(QObject *parent=0);
  /** Sends a request to the given host and port. */
  bool request(const QString &name="pool.ntp.org", uint16_t port=123);
  /** Sends a request to the given host and port. */
  bool request(const QHostAddress &addr, uint16_t port=123);
  /** Returns the received local clock offset in ms. */
  qint64 offset() const;

signals:
  /** Gets emitted when the NTP response is received. */
  void received(int64_t offset);
  /** Gets emitted when the NTP request timeout. */
  void timeout();

public:
  /** Blocking request for clock offset. */
  static qint64 getOffset(const QString &name="pool.ntp.org", uint16_t port=123);

protected slots:
  /** Gets called on reception of a UDP datagram. */
  void onDatagramReceived();

protected:
  /** The UDP socket. */
  QUdpSocket _socket;
  /** The timeout timer. */
  QTimer     _timer;
  /** The received clock offset in ms. */
  qint64     _offset;
};

#endif
