#include "secureshell.h"

SecureShell::SecureShell(DHT &dht, const QString &command, QObject *parent)
  : SecureStream(dht, parent), _command(command), _process()
{
  connect(&_process, SIGNAL(started()), this, SLOT(_shellStarted()));
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

void
SecureShell::close() {
  SecureStream::close();
  _process.terminate();
}

void
SecureShell::_shellStarted() {
  // pass...
}

void
SecureShell::_shellReadyRead() {
  char buffer[256];
  while (_process.bytesAvailable()) {
    qint64 len = 256;
    len = _process.read(buffer, len);
    write(buffer, len);
  }
}

void
SecureShell::_shellBytesWritten(qint64 bytes) {
  char buffer[256];
  while (bytesAvailable()) {
    qint64 len = 256;
    len = read(buffer, len);
    _process.write(buffer, len);
  }
}

void
SecureShell::_remoteReadyRead() {
  char buffer[256];
  while (bytesAvailable()) {
    qint64 len = 256;
    len = read(buffer, len);
    _process.write(buffer, len);
  }
}

void
SecureShell::_remoteBytesWritten(qint64 bytes) {
  char buffer[256];
  while (_process.bytesAvailable()) {
    qint64 len = 256;
    len = _process.read(buffer, len);
    write(buffer, len);
  }
}

void
SecureShell::_remoteClosed() {
  _process.terminate();
}




