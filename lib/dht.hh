#ifndef __OVL_DHT_H__
#define __OVL_DHT_H__

#include "buckets.hh"

#include <inttypes.h>

#include <QObject>
#include <QUdpSocket>
#include <QPair>
#include <QVector>
#include <QSet>
#include <QTimer>

// Forward declarations
struct Message;
class SearchQuery;
class Request;
class PingRequest;
class FindNodeRequest;
class FindValueRequest;
class FindNeighboursRequest;
class RendezvousSearchRequest;
class StartConnectionRequest;
class Identity;
class AbstractService;
class ServiceHandler;
class SecureSocket;


/** Implements a node in the DHT. */
class DHT: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param id Weak reference to the identity of the node.
   * @param streamHandler Specifies the socket-handler.
   * @param addr Specifies the network address the node will bind to.
   * @param port Specifies the network port the node will listen on.
   * @param parent Optional pararent object. */
  explicit DHT(Identity &id, const QHostAddress &addr=QHostAddress::Any,
               quint16 port=7741, QObject *parent=0);

  /** Destructor. */
  virtual ~DHT();

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

  /** Returns the number of nodes in the buckets. */
  size_t numNodes() const;
  /** Sends a ping request to the given hostname and port. */
  void ping(const QString &addr, uint16_t port);
  /** Sends a ping request to the given peer. */
  void ping(const QHostAddress &addr, uint16_t port);
  /** Sends a ping request to the given peer. */
  void ping(const PeerItem &peer);
  /** Sends a ping request to the given node. */
  void ping(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** Sends a ping request to the given node. */
  void ping(const NodeItem &node);

  /** Returns the list of all nodes in the buckets. */
  void nodes(QList<NodeItem> &lst);

  /** Searches for the neighbours of the given identifier. */
  void findNeighbours(const Identifier &id, const QList<NodeItem> &start = QList<NodeItem>());
  /** Starts the search for a node with the given identifier. */
  void findNode(const Identifier &id);
  /** Starts a rendezvous search for the given node.
   * First the neighbours of the node are searched and a rendezvous request will be send to each of
   * the neighbours. */
  void rendezvous(const Identifier &id);

  /** Returns @c true if a handler is associated with the given service. */
  bool hasService(uint16_t service) const;
  /** Registers a service.
   * Returns @c true on success and @c false if a handler is already associated with the given
   * service. The ownership of the handler is transferred to the DHT. */
  bool registerService(uint16_t no, AbstractService *handler);
  /** Retunrs the number of active connections. */
  size_t numSockets() const;

  /** Starts a secure connection.
   * The ownership of the @c SecureSocket instance is passed to the DHT and will be deleted if the
   * connection fails. If the connection is established, the ownership of the socket is passed to
   * the serivce handler instance. */
  bool startConnection(uint16_t service, const NodeItem &node, SecureSocket *stream);
  /** Unregister the socket with the DHT instance. */
  void socketClosed(const Identifier &id);
  
signals:
  /** Gets emitted as the DHT node enters the network. */
  void connected();
  /** Gets emitted as the DHT node leaves the network. */
  void disconnected();
  /** Gets emitted if a node leaves the buckets. */
  void nodeLost(const Identifier &id);
  /** Gets emitted if a node enters the bucketlist. */
  void nodeAppeard(const NodeItem &node);

  /** Gets emitted if a ping was replied. */
  void nodeReachable(const NodeItem &node);
  /** Gets emitted if the given node has been found. */
  void nodeFound(const NodeItem &node);
  /** Gets emitted if the given node was not found. */
  void nodeNotFound(const Identifier &id, const QList<NodeItem> &best);
  /** Gets emitted if a search neighbours query finished. */
  void neighboursFound(const Identifier &id, const QList<NodeItem> &neighbours);
  /** Gets emitted if the node to date can be found. */
  void rendezvousInitiated(const NodeItem &node);
  /** Gets emitted if the node to date cannot be found. */
  void rendezvousFailed(const Identifier &id);

protected:
  /** Sends a FindNode message to the node @c to to search for the node specified by @c id.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindNode(const NodeItem &to, SearchQuery *query);
  /** Sends a FindNode message to the node @c to to search for neighbours. */
  void sendFindNeighbours(const NodeItem &to, SearchQuery *query);
  /** Sends a RendezvousSearchRequest to the given node. */
  void sendRendezvousSearch(const NodeItem &to, SearchQuery *query);
  /** Sends a Rendezvous request to the given node. */
  void sendRendezvous(const Identifier &with, const PeerItem &to);
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
  void _processFindNodeResponse(const Message &msg, size_t size, FindNodeRequest *req,
                                const QHostAddress &addr, uint16_t port);
  /** Processes a FindNeighbours response. */
  void _processFindNeighboursResponse(const Message &msg, size_t size, FindNeighboursRequest *req,
                                      const QHostAddress &addr, uint16_t port);
  /** Processes a RendezvousSearchRequest response. */
  void _processRendezvousSearchResponse(const Message &msg, size_t size, RendezvousSearchRequest *req,
                                        const QHostAddress &addr, uint16_t port);
  /** Processes a StartStream response. */
  void _processStartConnectionResponse(const Message &msg, size_t size, StartConnectionRequest *req,
                                   const QHostAddress &addr, uint16_t port);
  /** Processes a Ping request. */
  void _processPingRequest(const Message &msg, size_t size,
                           const QHostAddress &addr, uint16_t port);
  /** Processes a FindNode request. */
  void _processFindNodeRequest(const Message &msg, size_t size,
                               const QHostAddress &addr, uint16_t port);
  /** Processes an Announce request. */
  void _processAnnounceRequest(const Message &msg, size_t size,
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
  /** Gets called regularily to check the timeout of the node in the buckets. */
  void _onCheckNodeTimeout();
  /** Gets called regularily to update the statistics. */
  void _onUpdateStatistics();
  /** Gets called when some data has been send. */
  void _onBytesWritten(qint64 n);
  /** Gets called on socket errors. */
  void _onSocketError(QAbstractSocket::SocketState error);

protected:
  /** The identifier of the node. */
  Identity &_self;
  /** The network socket. */
  QUdpSocket _socket;
  /** If @c true, the socket was bound to the address and port given to the constructor. */
  bool _started;

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

  /** The routing table. */
  Buckets _buckets;

  /** The list of pending requests. */
  QHash<Identifier, Request *> _pendingRequests;

  /** Table of services. */
  QHash<uint16_t, AbstractService *> _services;
  /** The list of open connection. */
  QHash<Identifier, SecureSocket *> _connections;

  /** Timer to check timeouts of requests. */
  QTimer _requestTimer;
  /** Timer to check nodes in buckets. */
  QTimer _nodeTimer;
  /** Timer to update i/o statistics every 5 seconds. */
  QTimer _statisticsTimer;

  // Allow SecureSocket to access sendData()
  friend class SecureSocket;
};



#endif // __OVL_DHT_H__
