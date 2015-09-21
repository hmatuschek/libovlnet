#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QHostInfo>
#include "qntp/NtpClient.h"


class Application : public QApplication
{
  Q_OBJECT

public:
  explicit Application(int &argc, char **argv);

public slots:
  void updateClockOffset();

protected slots:
  void onNTPAddrResolved(const QHostInfo &host);
  void onNTPUpdate(const QHostAddress &address, quint16 port, const NtpReply &reply);

protected:
  NtpClient _ntpClient;
  qint64 _clockOffset;
};

#endif // APPLICATION_H
