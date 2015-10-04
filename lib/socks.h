#ifndef SOCKS_H
#define SOCKS_H

#include "crypto.h"
#include "stream.h"


/** Implements a local enpoint of a SOCKS v4 or v5 connection that will be relayed to another
 * node which acts as the exit point. This class is simple, once the connection to the node is
 * established, the data to and from the local TCP connection is forwarded to the node, including
 * the SOCKS messages. It is the remote node that implements the actual SOCKS proxy. */
class SOCKSInStream: public SecureStream
{
  Q_OBJECT

public:
  /** Constructs a local SOCK input stream.
   * @param dht A weak reference to the DHT instance.
   * @param instream The local incomming TCP-SOCKS connection.
   * @param parent The optional QObject parent. */
  SOCKSInStream(DHT &dht, QTcpSocket *instream, QObject *parent=0);
  /** Destructor, closes the TCP and @c SecureStream connections. */
  virtual ~SOCKSInStream();

  /** Gets called once the connection to the remote node is established. */
  bool open(OpenMode mode);

protected slots:
  /** Gets called if data arrived from the client. */
  void _clientReadyRead();
  /** Gets called if data has been send to the client. */
  void _clientBytesWritten(qint64 bytes);
  /** Gets called if the connection to the client is closed. */
  void _clientDisconnected();
  /** Gets called on error. */
  void _clientError(QAbstractSocket::SocketError error);

  /** Gets called if data has been arrived from the remote node. */
  void _remoteReadyRead();
  /** Gets called if data has been send to the remote node. */
  void _remoteBytesWritten(qint64 bytes);
  /** Gets called if the connection to the remote node is closed. */
  void _remoteClosed();

protected:
  QTcpSocket *_inStream;
};


/** Represents the exit point of a SOCKS v5 prox connection.
 * Once the connection with the client is made, the SOCKS request is parsed and a connection
 * to the requested host is established. Any further data to and from the client is then
 * proxied to that host. */
class SOCKSOutStream: public SecureStream
{
  Q_OBJECT

public:
  /** Connection and SOCKS request parser state. */
  typedef enum {
    RX_VERSION,
    RX_AUTHENTICATION,
    RX_REQUEST,
    RX_REQUEST_ADDR_IP4,
    RX_REQUEST_ADDR_NAME_LEN,
    RX_REQUEST_ADDR_NAME,
    RX_REQUEST_ADDR_IP6,
    RX_REQUEST_PORT,
    CONNECTING,
    STARTED,
    CLOSED
  } State;

public:
  /** Constructor.
   * @param dht A weak reference to the local DHT node instance.
   * @param parent The optional QObject parent. */
  SOCKSOutStream(DHT &dht, QObject *parent=0);
  /** Destructor, also cloeses the connections. */
  virtual ~SOCKSOutStream();

  /** Gets called once the connection to the peer node is established. */
  bool open(OpenMode mode);

protected slots:
  /** SOCKS request parser. */
  void _clientParse();
  /** Gets called if data arrived from the peer. */
  void _clientReadyRead();
  /** Gets called if data has been send to the peer. */
  void _clientBytesWritten(qint64 bytes);
  /** Gets called if the connection to the client is peer. */
  void _clientClosed();

  void _remoteConnected();
  /** Gets called if data has been arrived from the remote host. */
  void _remoteReadyRead();
  /** Gets called if data has been send to the remote host. */
  void _remoteBytesWritten(qint64 bytes);
  /** Gets called if the connection to the remote host is closed. */
  void _remoteDisconnected();
  /** Gets called on error. */
  void _remoteError(QAbstractSocket::SocketError error);

protected:
  State _state;
  /** The connection to the remote host. */
  QTcpSocket *_outStream;
  /** Number of bytes to receive for the SOCKS v5 authentication methods. */
  size_t _nAuthMeth;
  /** The received SOCKS5 authentication methods. */
  QString _authMeth;
  /** The remote host address. */
  QHostAddress _addr;
  /** The remote host name length. */
  size_t _nHostName;
  /** The remote host name. */
  QString _hostName;
  /** The port of the remote host to connect to. */
  uint16_t _port;
};


#endif // SOCKS_H
