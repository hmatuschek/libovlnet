#include "secureshell.h"

SecureShell::SecureShell(DHT &dht, const QString &command, QObject *parent)
  : SecureStream(dht, parent), _command(command), _process()
{
  conenct(&_process, SIGNAL(started()), this, SLOT(_shellStarted()));
  connect(&_process, SIGNAL(readyReadStandardOutput()), this, SLOT(_shellReadyRead()));
  connect(&_process, SIGNAL(readyReadStandardError()), this, SLOT(_shellReadyRead()));
  connect(&_process, SIGNAL(bytesWritten(qint64)), this, SLOT(_shellBytesWritten(qint64)));

  connect(this, SIGNAL(readyRead()), this, SLOT(_remoteReadyRead()));
  connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(_remoteBytesWritten(qint64)));
  connect(this, SIGNAL(aboutToClose()), this, SLOT(_remoteClosed()));
}

bool
SecureShell::open(OpenMode mode) {
  if (! SecureStream::open(mode)) { return false; }
  _process.start(_command);
  return true;
}







