#ifndef SECURESHELL_H
#define SECURESHELL_H

#include "stream.h"
#include <QProcess>


class SecureShell : public SecureStream
{
  Q_OBJECT

public:
  SecureShell(DHT &dht, const QString &command="login", QObject *parent=0);

  bool open(OpenMode mode);

protected slots:
  void _shellStarted();
  void _shellReadyRead();
  void _shellBytesWritten(qint64 bytes);

  void _remoteReadyRead();
  void _remoteBytesWritten(qint64 bytes);
  void _remoteClosed();

protected:
  QString  _command;
  QProcess _process;
};

#endif // SECURESHELL_H
