#ifndef SOCKSSERVICE_H
#define SOCKSSERVICE_H

#include "lib/dht.h"
#include "lib/socks.h"

#include <inttypes.h>
#include <QObject>
#include <QTcpServer>

class Application;


/** Self destructing variant of @c SOCKSInStream.
 * This class extends @c SOCKSInStream by deleting the instance if either the TCP or the
 * connection to the remote node is closed. */
class SocksConnection: public SOCKSLocalStream
{
  Q_OBJECT

public:
  /** Constructor.
   * @param dht A weak reference to the DHT.
   * @param instream The local TCP connection.
   * @param parent The optional QObject parent. */
  SocksConnection(DHT &dht, QTcpSocket *instream, QObject *parent=0);

  /** Also destructs this instance. */
  void close();
};


/** This class implements a simple SOCKS v5 proxy server that relays the requests to another
 * node providing the SOCKS service. The node is passed to the constructor.
 * This class listen on a local TCP port (1080 by default) for incomming connections. Once a
 * TCP connection is established, the remote node is contacted. If the connection to the remote
 * node is established, the client (at the TCP connection) can use the remote node as a proxy. */
class SocksService : public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param app Specifies the application instance (holding the DHT instance etc.).
   * @param remote The remote node to use as a proxy.
   * @param port Specifies the local TCP port to listen for incomming connections.
   * @param parent The optional QObject parent. */
  explicit SocksService(Application &app, const NodeItem &remote, uint16_t port=1080, QObject *parent = 0);

  /** Destructor. */
  virtual ~SocksService();

  /** Retruns true if the server is running. */
  bool isListening() const;
  /** Returns the current number of active proxy connections. */
  size_t connectionCount() const;

signals:
  void connectionCountChanged(size_t count);

protected slots:
  /** Handles incomming TCP connections. */
  void _onNewConnection();
  void _onConnectionClosed();

protected:
  /** A weak reference to the application. */
  Application &_application;
  /** The remote node acting as a proxy. */
  NodeItem _remote;
  /** The local TCP server waiting for incomming connections. */
  QTcpServer _server;
  /** Holds the current connection count. */
  size_t _connectionCount;
};

#endif // SOCKSSERVICE_H
