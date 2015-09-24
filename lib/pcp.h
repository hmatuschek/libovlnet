#ifndef __VLF_PCP_H__
#define __VLF_PCP_H__

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <inttypes.h>

class PCPClient: public QObject
{
  Q_OBJECT

public:
  explicit PCPClient(QObject *parent=0);

public slots:
  void requestMap(uint16_t iport, const QHostAddress &addr, uint16_t port=5351);

protected slots:
  void _onDatagramReceived();

protected:
  uint8_t _nonce[12];
  QUdpSocket _socket;
};

#endif // __VLF_PCP_H__
