#ifndef __OVL_DHT_H__
#define __OVL_DHT_H__

#include "buckets.hh"
#include "crypto.hh"

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
class FindNodeRequest;
class FindValueRequest;
class AnnounceRequest;
class FindNeighboursRequest;
class RendezvousSearchRequest;
class StartConnectionRequest;
class AbstractService;
class ServiceHandler;
class SecureSocket;

/** Base class of all search queries.
 * Search queryies are used to keep track of nodes and values that can be found in the OVL
 * network. */
class SearchQuery
{
public:
  /** Constructor. */
  SearchQuery(const Identifier &id);

  /** Destructor. */
  virtual ~SearchQuery();

  /** Ignore the following node ID. */
  void ignore(const Identifier &id);

  /** Returns the identifier of the element being searched for. */
  const Identifier &id() const;

  /** Update the search queue (ordered list of nodes to query). */
  void update(const NodeItem &nodes);

  /** Returns the next node to query or @c false if no node left to query. */
  bool next(NodeItem &node);

  /** Returns the current search query. This list is also the list of the closest nodes to the
   * target known. */
  QList<NodeItem> &best();
  /** Returns the current search query. This list is also the list of the closest nodes to the
   * target known. */
  const QList<NodeItem> &best() const;

  /** Returns the first element from the search queue. */
  const NodeItem &first() const;

  /** Gets called if the search query failed.
   * This will delete the search query instance. */
  virtual void succeeded();

  /** Gets called if the search query failed.
   * This will delete the search query instance. */
  virtual void failed();

protected:
  /** The identifier of the element being searched for. */
  Identifier _id;
  /** The current search queue. */
  QList<NodeItem> _best;
  /** The set of nodes already asked. */
  QSet<Identifier> _queried;
};


/** A specialization of @c SearchQuery that is used to search for values.
 * Additionally to the @c SearchQuery, the @c ValueSearchQuery can hold a list of peers that
 * provide the value being searched for. */
class ValueSearchQuery: public SearchQuery
{
public:
  /** Constructor.
   * @param id Specifies the identifier of the value begin searched. */
  ValueSearchQuery(const Identifier &id);
  /** Destructor. */
  virtual ~ValueSearchQuery();
  /** Adds a peer to the list of nodes holding the value. */
  void addPeer(const PeerItem &peer);
  /** Returns a reference to the list of peers holding the value. */
  QList<PeerItem> &peers();
  /** Returns a reference to the list of peers holding the value. */
  const QList<PeerItem> &peers() const;

protected:
  /** The list of peers holding the value. */
  QList<PeerItem> _peers;
};


/** Implements a node in the OVL network. */
class Node: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param idFile Filename to read the identity of the node from.
   * @param addr Specifies the network address the node will bind to.
   * @param port Specifies the network port the node will listen on.
   * @param parent Optional pararent object. */
  explicit Node(const QString &idFile, const QHostAddress &addr=QHostAddress::Any,
               quint16 port=7741, QObject *parent=0);

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

  /** Starts the search for a node with the given identifier. */
  void findNode(const Identifier &id);
  /** Starts the search for a node with the query. */
  void findNode(SearchQuery *query);

  /** Searches for the neighbours of the given identifier. */
  void findNeighbours(const Identifier &id, const QList<NodeItem> &start = QList<NodeItem>());
  /** Searches for the neighbours of the node specified by the search query. */
  void findNeighbours(SearchQuery *query, const QList<NodeItem> &start = QList<NodeItem>());

  /** Starts the search for a value with the given identifier. */
  void findValue(const Identifier &id);
  /** Starts the search for a value with the query. */
  void findValue(ValueSearchQuery *query);

  /** Starts the search for a value with the given identifier. */
  void announce(const Identifier &id);
  /** Removes an announcement. */
  void remAnnouncement(const Identifier &id);

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

  /** Returns @c true if a handler is associated with the given service name. */
  bool hasService(const char *service) const;
  /** Returns @c true if a handler is associated with the given service identifier. */
  bool hasService(const Identifier &service) const;
  /** Registers a service.
   * Returns @c true on success and @c false if a handler is already associated with the given
   * service. The ownership of the handler is transferred to the DHT. */
  bool registerService(const QString& service, AbstractService *handler);

  /** Retunrs the number of active connections. */
  size_t numSockets() const;
  /** Starts a secure connection.
   * The ownership of the @c SecureSocket instance is passed to the @c Node and will be deleted if the
   * connection fails. If the connection is established, the ownership of the socket is passed to
   * the serivce handler instance. */
  bool startConnection(const QString &service, const NodeItem &node, SecureSocket *stream);
  /** Unregister a socket with the Node instance. */
  void socketClosed(const Identifier &id);
  
signals:
  /** Gets emitted as the Node enters the network. */
  void connected();
  /** Gets emitted as the Node leaves the network. */
  void disconnected();
  /** Gets emitted if a node leaves the buckets. */
  void nodeLost(const Identifier &id);
  /** Gets emitted if a node enters the buckets. */
  void nodeAppeard(const NodeItem &node);

  /** Gets emitted if a ping was replied. */
  void nodeReachable(const NodeItem &node);

  /** Gets emitted if the given node has been found. */
  void nodeFound(const NodeItem &node);
  /** Gets emitted if the given node was not found. */
  void nodeNotFound(const Identifier &id, const QList<NodeItem> &best);

  /** Gets emitted if the given value has been found. */
  void valueFound(const Identifier &id, const QList<NodeItem> &nodes);
  /** Gets emitted if the given value was not found. */
  void valueNotFound(const Identifier &id, const QList<NodeItem> &best);

  /** Gets emitted if a search for neighbours finished. */
  void neighboursFound(const Identifier &id, const QList<NodeItem> &neighbours);

  /** Gets emitted if the node to date has been notified. */
  void rendezvousInitiated(const NodeItem &node);
  /** Gets emitted if the node to date cannot be found. */
  void rendezvousFailed(const Identifier &id);

protected:
  /** Sends a FindNode message to the node @c to to search for the node specified by the @c query.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindNode(const NodeItem &to, SearchQuery *query);
  /** Sends a FindNode message to the node @c to to search for neighbours. */
  void sendFindNeighbours(const NodeItem &to, SearchQuery *query);
  /** Sends a Announce message to the node @c to. */
  void sendAnnouncement(const NodeItem &to, SearchQuery *query);
  /** Sends a FindValue message to the node @c to to search for the valuespecified by @c id.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindValue(const NodeItem &to, ValueSearchQuery *query);
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
  /** Processes a FindValue response. */
  void _processFindValueResponse(const Message &msg, size_t size, FindValueRequest *req,
                                const QHostAddress &addr, uint16_t port);
  /** Processes a Announce response. */
  void _processAnnounceResponse(const Message &msg, size_t size, AnnounceRequest *req,
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
  /** Processes a FindValue request. */
  void _processFindValueRequest(const Message &msg, size_t size,
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
  /** Gets called periodically to ping the K nearest neighbours. They act as rendezvous nodes.
   * By pinging them regularily the "connection" to these nodes through a NAT will remain "open".
   * This allows to receive rendezvous messages from these nodes. */
  void _onPingRendezvousNodes();
  /** Gets called periodically to check for dead announcements and to check if my announcements
   * needed to be refreshed. */
  void _onCheckAnnouncements();
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

  /** The routing table of the OVL network. */
  Buckets _buckets;

  /** Hash table of announced items. */
  QHash<Identifier, QSet<AnnouncementItem> > _hashTable;
  /** Table of items to announce. */
  QHash<Identifier, QDateTime> _annouceItems;

  /** The list of pending requests. */
  QHash<Identifier, Request *> _pendingRequests;

  /** Table of services. */
  QHash<Identifier, AbstractService *> _services;
  /** The list of open connection. */
  QHash<Identifier, SecureSocket *> _connections;

  /** Timer to check timeouts of requests. */
  QTimer _requestTimer;
  /** Timer to check nodes in buckets. */
  QTimer _nodeTimer;
  /** Timer to ping rendezvous nodes. */
  QTimer _rendezvousTimer;
  /** Timer to update announcements. */
  QTimer _announceTimer;
  /** Timer to update i/o statistics every 5 seconds. */
  QTimer _statisticsTimer;

  // Allow SecureSocket to access sendData()
  friend class SecureSocket;
};



#endif // __OVL_DHT_H__
