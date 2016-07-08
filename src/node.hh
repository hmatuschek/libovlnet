#ifndef __OVL_DHT_H__
#define __OVL_DHT_H__

#include "crypto.hh"
#include "network.hh"

#include <inttypes.h>

#include <QObject>
#include <QUdpSocket>
#include <QPair>
#include <QVector>
#include <QSet>
#include <QTimer>

// Forward declarations
struct Message;
class Request;
class PingRequest;
class SearchRequest;
class StartConnectionRequest;
class AbstractService;
class ServiceHandler;
class SecureSocket;


/** Implements a node in the OVL network. */
class Node: public Network
{
  Q_OBJECT

public:
  /** Constructor.
   * @param id Weak reference to the identity of the node.
   * @param addr Specifies the network address the node will bind to.
   * @param port Specifies the network port the node will listen on.
   * @param parent Optional pararent object. */
  explicit Node(const Identity &id, const QHostAddress &addr=QHostAddress::Any,
               quint16 port=7741, QObject *parent=0);


  /** Destructor. */
  virtual ~Node();

  /** Returns a weak reference it the identity of the node. */
  Identity &identity();
  /** Returns a weak reference it the identity of the node. */
  const Identity &identity() const;
  /** Returns the identifier of the DHT node. */
  const Identifier &id() const;
  /** Returns @c true if the socket is listening on the specified port. */
  bool started() const;

  /** Returns the number of bytes send. */
  size_t bytesSend() const;
  /** Returns the number of bytes received. */
  size_t bytesReceived() const;
  /** Returns the download rate. */
  double inRate() const;
  /** Returns the upload rate. */
  double outRate() const;

  /** Returns a weak reference to the root network node. */
  Node &root();
  /** Returns the network name. Here the empty string. */
  const QString &prefix() const;
  /** Returns @c true if the given network is registered. */
  bool hasNetwork(const QString &prefix) const;
  /** Registers a network with the node. */
  bool registerNetwork(Network *subnet);
  /** Returns the network registered with the given prefix. */
  Network *network(const QString &prefix);

  /** Sends a ping to the given hostname and port.
   * On success, the @c nodeReachable signal gets emitted. */
  void ping(const QString &addr, uint16_t port);
  /** Sends a ping to the given peer.
   * On success, the @c nodeReachable signal gets emitted. */
  void ping(const QHostAddress &addr, uint16_t port);
  /** Sends a ping to the given peer.
   * On success, the @c nodeReachable signal gets emitted. */
  void ping(const PeerItem &peer);
  /** Sends a ping to the given node.
   * On success, the @c nodeReachable signal gets emitted.*/
  void ping(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** Sends a ping to the given node.
   * On success, the @c nodeReachable signal gets emitted. */
  void ping(const NodeItem &node);
  /** Returns the number of nodes in the buckets. */
  size_t numNodes() const;
  /** Returns the list of all nodes in the buckets. */
  void nodes(QList<NodeItem> &lst);

  /** Starts the search for a node with the query. */
  void search(SearchQuery *query);

  /** Starts a rendezvous search for the given node.
   * First the neighbours of the node are searched and a rendezvous request will be send to each of
   * the neighbours. */
  void rendezvous(const Identifier &id);
  /** Returns @c true if a rendezvous ping is send periodically to the K nearest neighbours.
   * This allows to receive out-of-band packages from them even if this node is behind a (coned)
   * NAT. This feature is not required for nodes being directly reachable. Disable it in these
   * cases using @c enableRendezvousPing. */
  bool rendezvousPingEnabled() const;
  /** Enables or disables the rendezvous ping to the nearest neighbours. */
  void enableRendezvousPing(bool enable);
  /** Sends a Rendezvous request to the given node. */
  void sendRendezvous(const Identifier &with, const PeerItem &to);

  /** Returns @c true if a handler is associated with the given service name. */
  bool hasService(const QString &service) const;
  /** Registers a service.
   * Returns @c true on success and @c false if a handler is already associated with the given
   * service. The ownership of the handler is transferred to the DHT. */
  bool registerService(const QString& service, AbstractService *handler);
  /** Returns @c true if a handler is associated with the given service identifier. */
  bool hasService(const Identifier &service) const;

  /** Retunrs the number of active connections. */
  size_t numSockets() const;
  /** Starts a secure connection.
   * The ownership of the @c SecureSocket instance is passed to the @c Node and will be deleted if the
   * connection fails. If the connection is established, the ownership of the socket is passed to
   * the serivce handler instance. */
  bool startConnection(const QString &service, const NodeItem &node, SecureSocket *stream);
  /** Unregister a socket with the Node instance. */
  void socketClosed(const Identifier &id);
  
protected:
  /** Sends a ping to the given peer to test if he is a member of the given network. */
  void sendPing(const Identifier &id, const QHostAddress &addr, uint16_t port, const Identifier &netid);
  /** Sends a ping to the given peer to test if he is a member of the given network. */
  void sendPing(const QHostAddress &addr, uint16_t port, const Identifier &netid);
  /** Sends a FindNode message to the node @c to to search for the node specified by the @c query.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendSearch(const NodeItem &to, SearchQuery *query);
  /** Sends some data with the given connection id. */
  bool sendData(const Identifier &id, const uint8_t *data, size_t len,
                const PeerItem &peer);
  /** Sends some data with the given connection id. */
  bool sendData(const Identifier &id, const uint8_t *data, size_t len,
                const QHostAddress &addr, uint16_t port);

private:
  /** Processes a Ping response. */
  void _processPingResponse(const Message &msg, size_t size, PingRequest *req,
                            const QHostAddress &addr, uint16_t port);
  /** Processes a FindNode response. */
  void _processSearchResponse(const Message &msg, size_t size, SearchRequest *req,
                              const QHostAddress &addr, uint16_t port);
  /** Processes a StartStream response. */
  void _processStartConnectionResponse(const Message &msg, size_t size, StartConnectionRequest *req,
                                   const QHostAddress &addr, uint16_t port);
  /** Processes a Ping request. */
  void _processPingRequest(const Message &msg, size_t size,
                           const QHostAddress &addr, uint16_t port);
  /** Processes a FindNode request. */
  void _processSearchRequest(const Message &msg, size_t size,
                             const QHostAddress &addr, uint16_t port);
  /** Processes a StartStream request. */
  void _processStartConnectionRequest(const Message &msg, size_t size,
                                  const QHostAddress &addr, uint16_t port);
  /** Processes a Rendezvous request. */
  void _processRendezvousRequest(Message &msg, size_t size,
                                 const QHostAddress &addr, uint16_t port);

private slots:
  /** Gets called on the reception of a UDP package. */
  void _onReadyRead();
  /** Gets called regulary to check the request timeouts. */
  void _onCheckRequestTimeout();
  /** Gets called periodically to ping the K nearest neighbours. They act as rendezvous nodes.
   * By pinging them regularily the "connection" to these nodes through a NAT will remain "open".
   * This allows to receive rendezvous messages from these nodes. */
  void _onPingRendezvousNodes();
  /** Gets called regularily to update the statistics. */
  void _onUpdateStatistics();
  /** Gets called when some data has been send. */
  void _onBytesWritten(qint64 n);
  /** Gets called on socket errors. */
  void _onSocketError(QAbstractSocket::SocketState error);

protected:
  /** The identifier of the node. */
  Identity _self;
  /** The network socket. */
  QUdpSocket _socket;
  /** If @c true, the socket was bound to the address and port given to the constructor. */
  bool _started;

  /** Empty string netid. */
  QString _prefix;
  /** Table of networks. */
  QHash<Identifier, Network *> _networks;

  /** The number of bytes received. */
  size_t _bytesReceived;
  /** The number of bytes received at the last update. */
  size_t _lastBytesReceived;
  /** The input rate. */
  double _inRate;

  /** The number of bytes send. */
  size_t _bytesSend;
  /** The number of bytes send at the last update. */
  size_t _lastBytesSend;
  /** The output rate. */
  double _outRate;

  /** The list of pending requests. */
  QHash<Identifier, Request *> _pendingRequests;

  /** Table of services. */
  QHash<Identifier, AbstractService *> _services;
  /** The list of open connection. */
  QHash<Identifier, SecureSocket *> _connections;

  /** Timer to check timeouts of requests. */
  QTimer _requestTimer;
  /** Timer to ping rendezvous nodes. */
  QTimer _rendezvousTimer;
  /** Timer to update i/o statistics every 5 seconds. */
  QTimer _statisticsTimer;

  // Allow SecureSocket to access sendData()
  friend class SecureSocket;
  friend class SubNetwork;
};


class RendezvousSearchQuery: public NeighbourhoodQuery
{
  Q_OBJECT

public:
  RendezvousSearchQuery(Node &node, const Identifier &id);

  bool next(NodeItem &node);

protected:
  Node &_node;
};



#endif // __OVL_DHT_H__
