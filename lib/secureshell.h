#ifndef SECURESHELL_H
#define SECURESHELL_H

#include "stream.h"
#include <QProcess>

/** Implements a trivial secure shell service.
 * This class simply forwards any data to and from a @c SecureStream connection to a process.
 * @ingroup services */
class SecureShell : public SecureStream
{
  Q_OBJECT

public:
  /** Constructor.
   * @param dht Specifies the DHT instance.
   * @param command Specifies the command to execute.
   * @param parent Specifies the optional QObject parent. */
  SecureShell(DHT &dht, const QString &command="login", QObject *parent=0);

  /** Startes the process. */
  bool open(OpenMode mode);
  /** Closes the connection and terminates the process. */
  void close();

protected slots:
  /** Gets called once the process started. */
  void _shellStarted();
  /** Gets called if some data get available from the process. */
  void _shellReadyRead();
  /** Gets called if some data has been written to the process. */
  void _shellBytesWritten(qint64 bytes);

  /** Gets called if some data has been received from the remote. */
  void _remoteReadyRead();
  /** Gets called if some data has been send to the remote. */
  void _remoteBytesWritten(qint64 bytes);
  /** Gets called if the connection to the remote has been closed. */
  void _remoteClosed();

protected:
  /** The command to execute. */
  QString  _command;
  /** The process. */
  QProcess _process;
};

#endif // SECURESHELL_H
