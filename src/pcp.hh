#ifndef __OVL_PCP_H__
#define __OVL_PCP_H__

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <inttypes.h>

/** Implements a simple PCP client.
 * @ingroup nat */
class PCPClient: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param parent Specifies the optional QObject parent. */
  explicit PCPClient(QObject *parent=0);

public slots:
  /** Requests a mapping for @c iport from the PCP server at @c addr and @c port. */
  void requestMap(uint16_t iport, const QHostAddress &addr, uint16_t port=5351);

signals:
  /** Gets emitted if a mapping request was successful. Then a mapping from @c iport
   * to @c eport at @c eaddr was established. */
  void mapping(uint16_t iport, const QHostAddress &eaddr, uint16_t eport);

protected slots:
  /** Handles incomming UDP datagrams. */
  void _onDatagramReceived();

protected:
  /** Request nonce. */
  uint8_t _nonce[12];
  /** The local UDP socket. */
  QUdpSocket _socket;
};

#endif // __OVL_PCP_H__
