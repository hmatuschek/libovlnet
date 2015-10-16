#ifndef SOCKS_H
#define SOCKS_H

#include "crypto.h"
#include "stream.h"

#include <QTcpSocket>
#include <QTcpServer>


/** Implements a local enpoint of a SOCKS v4 or v5 connection (tunnel) that will be relayed to
 * another node which acts as the exit point. This class is simple, once the connection to the node
 * is established, the data to and from the local TCP connection is forwarded to the node,
 * including the SOCKS messages. It is the remote node that implements the actual SOCKS proxy. */
class SOCKSLocalStream: public SecureStream
{
  Q_OBJECT

public:
  /** Constructs a local SOCK stream.
   * @param dht A weak reference to the DHT instance.
   * @param instream The local incomming TCP-SOCKS connection. The ownership of the socket is taken
   *        by the instance.
   * @param parent The optional QObject parent. */
  SOCKSLocalStream(DHT &dht, QTcpSocket *instream, QObject *parent=0);
  /** Destructor, closes the TCP and @c SecureStream connections. */
  virtual ~SOCKSLocalStream();

  /** Gets called once the connection to the remote node is established. */
  bool open(OpenMode mode);

  /** Closes the connection and queues the connection instance for destruction. */
  void close();

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
  /** The TCP connection to the client. */
  QTcpSocket *_inStream;
};


/** This class implements a simple local SOCKS v5 proxy server that relays the requests to another
 * node providing the SOCKS service. The node is passed to the constructor.
 * This class listen on a local TCP port (1080 by default) for incomming connections. Once a
 * TCP connection is established, the remote node is contacted. If the connection to the remote
 * node is established, the client (at the TCP connection) can use the remote node as a proxy. */
class LocalSocksService : public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * The constructor starts a TCP server bound to the localhost IPv4 address and listening on
   * the specified port (1080 by default).
   * @param dht Specifies the DHT node instance.
   * @param remote The remote node to use as a proxy.
   * @param port Specifies the local TCP port to listen for incomming connections.
   * @param parent The optional QObject parent. */
  explicit LocalSocksService(DHT &dht, const NodeItem &remote, uint16_t port=1080, QObject *parent = 0);

  /** Destructor. */
  virtual ~LocalSocksService();

  /** Retruns true if the server is running. */
  bool isListening() const;

  /** Returns the current number of active proxy connections. */
  size_t connectionCount() const;

signals:
  /** Gets emitted if the connection count changed. */
  void connectionCountChanged(size_t count);

protected slots:
  /** Handles incomming TCP connections. */
  void _onNewConnection();
  /** Gets called if a TCP connection is closed. */
  void _onConnectionClosed();

protected:
  /** A weak reference to the DHT node. */
  DHT &_dht;
  /** The remote node acting as a proxy. */
  NodeItem _remote;
  /** The local TCP server waiting for incomming connections. */
  QTcpServer _server;
  /** Holds the current connection count. */
  size_t _connectionCount;
};


/** Represents the exit point of a SOCKS v5 proxy connection.
 * Once the connection with the client is made, the SOCKS request is parsed and a connection
 * to the requested host is established. Any further data to and from the client is then
 * proxied to that host. */
class SOCKSOutStream: public SecureStream
{
  Q_OBJECT

public:
  /** Connection and SOCKS request parser states. */
  typedef enum {
    RX_VERSION,                ///< Initial state, parse SOCKS version number.
    RX_AUTHENTICATION,         ///< Parse authentication methods.
    RX_REQUEST,                ///< Parse request header.
    RX_REQUEST_ADDR_IP4,       ///< Parse remote address as IPv4 address.
    RX_REQUEST_ADDR_NAME_LEN,  ///< Parse remote host name length.
    RX_REQUEST_ADDR_NAME,      ///< Parse remote host name.
    RX_REQUEST_ADDR_IP6,       ///< Parse remote address as IPv6 address.
    RX_REQUEST_PORT,           ///< Parse remote port.
    CONNECTING,                ///< Connecting to the remote host & port.
    CONNECTED,                   ///< Connection established.
    CLOSING,                   ///< Finalizes the connection, tries to empty the buffers and
                               ///  closes the connection when done.
    CLOSED                     ///< Connection closed.
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

  /** Gets called if the TCP connection to the remote host is established. */
  void _remoteConnected();
  /** Gets called if data has been arrived from the remote host. */
  void _remoteReadyRead();
  /** Gets called if data has been send to the remote host. */
  void _remoteBytesWritten(qint64 bytes);
  void _remoteReadChannelFinished();
  /** Gets called if the connection to the remote host is closed, e.g. the remote resets the
   * TCP connection. */
  void _remoteDisconnected();
  /** Gets called on error, e.g. connection problems with the remote host. */
  void _remoteError(QAbstractSocket::SocketError error);

protected:
  /** The state of the parser & connection. */
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
