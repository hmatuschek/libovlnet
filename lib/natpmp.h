#ifndef NATPMPCLIENT_H
#define NATPMPCLIENT_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>


class NATPMPClient : public QObject
{
  Q_OBJECT

public:
  explicit NATPMPClient(QObject *parent = 0);

public slots:
  void requestMap(uint16_t iport, const QHostAddress &addr, uint16_t port=5351);

signals:

protected slots:
  void _onDatagramReceived();

protected:
  QUdpSocket _socket;
};

#endif // NATPMPCLIENT_H
