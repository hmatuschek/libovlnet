#ifndef __VLF_NTP_TIMESTAMP_H__
#define __VLF_NTP_TIMESTAMP_H__

#include <QObject>
#include <QUdpSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QString>


class NTPClient: public QObject
{
  Q_OBJECT

public:
  explicit NTPClient(QObject *parent=0);

  bool request(const QString &name="pool.ntp.org", uint16_t port=123);
  bool request(const QHostAddress &addr, uint16_t port=123);

signals:
  void received(int64_t offset);

protected slots:
  void onDatagramReceived();

protected:
  QUdpSocket _socket;
};

#endif
