#include "node.hh"
#include "crypto.hh"
#include "dht_config.hh"

#include <QHostInfo>
#include <netinet/in.h>
#include <inttypes.h>


/** Represents a triple of ID, IP address and port as transferred via UDP.
 * @ingroup internal */
struct __attribute__((packed)) DHTTriple {
  /** The ID of a node. */
  char      id[OVL_HASH_SIZE];
  /** The IP (IPv6) of the node. */
  uint8_t   ip[16];
  /** The port of the node. */
  uint16_t  port;
};


/** The structure of the UDP datagrams transferred.
 * @ingroup internal */
struct __attribute__((packed)) Message
{
  /** Possible message types. */
  typedef enum {
    /** A ping request or response. */
    PING = 0,
    /** A find node request message. */
    FIND_NODE,
    /** A find value request message. */
    FIND_VALUE,
    /** An announce indication. */
    ANNOUNCE,
    /** A "connect" request or response message. */
    CONNECT,
    /** A rendezvous request or notification message. */
    RENDEZVOUS,
  } Type;

  /** The magic cookie to match a response to a request. */
  char cookie[OVL_COOKIE_SIZE];

  /** Payload. */
  union __attribute__((packed)) {
    /** A ping message (request & response) consists of a type identifier
     * and the ID of the sender. */
    struct __attribute__((packed)){
      uint8_t type;               ///< Type flag == @c MSG_PING.
      char    id[OVL_HASH_SIZE];  ///< Identifier of the sender
    } ping;

    /** A find node request. */
    struct __attribute__((packed)) {
      /** Type flag == @c MSG_FIND_NODE | @c MSG_FIND_VALUE | @c MSG_ANNOUNCE. */
      uint8_t type;
      /** The identifier of the node to find. */
      char    id[OVL_HASH_SIZE];
      /** This dummy payload is needed to avoid the risk to exploid this request for a relay DoS
       * attack. It ensures that the request has at least the same size as the response. The size
       * of this field implicitly defines the maximal number of triples returned by the remote node. */
      char    dummy[OVL_MAX_TRIPLES*OVL_TRIPLE_SIZE-2*OVL_HASH_SIZE];
    } search;

    /** A response to "find value", "find node" or "announce". */
    struct __attribute__((packed)) {
      /** If a FIND_VALUE response, this flag indicates success. Then, the triples contain the
       * nodes associated with the requested value. Otherwise the triples contain the nodes to
       * query next. */
      uint8_t   success;
      /** Node tiples (ID, address, port). */
      DHTTriple triples[OVL_MAX_TRIPLES];
    } result;

    /** A start secure connection request and response. Implementing the ECDH handshake. */
    struct __attribute__((packed)) {
      /** Type flag == @c MSG_START_CONNECTION. */
      uint8_t  type;
      /** A service id (not part of the OVL specification). */
      uint8_t service[OVL_HASH_SIZE];
      /** Public (ECDH) key of the requesting or responding node, session public keys and lengths. */
      uint8_t  pubkey[OVL_MAX_PUBKEY_SIZE];
    } start_connection;

    /** A rendezvous request or notification message. */
    struct __attribute__((packed)) {
      /** Type flag == @c MSG_RENDEZVOUS. */
      uint8_t  type;
      /** Specifies the ID of the node to date. */
      char     id[OVL_HASH_SIZE];
      /** Will be set by the rendezvous server to the source address of the reuquest sender, 
       * before relaying it to the target. */
      char     ip[16];
      /** Will be set by the rendezvous server to the source port of the request sender, before 
       * relaying it to the target. */ 
      uint16_t port;
    } rendezvous; 

    /** A stream datagram. */
    uint8_t datagram[OVL_MAX_DATA_SIZE];
  } payload;

  /** Constructor. */
  Message();
};

Message::Message()
{
  memset(this, 0, sizeof(Message));
}


/** Base class of all request items. A request item will be stored for every request send. This
 * allows to associate a response (identified by the magic cookie) with a request.
 * @ingroup internal */
class Request
{
public:
  /** Request type. */
  typedef enum {
    PING,              ///< A ping request.
    FIND_NODE,         ///< A find node request.
    FIND_NEIGHBOURS,   ///< A find neighbours request.
    ANNOUNCEMENT,      ///< An announcement request.
    FIND_VALUE,        ///< A find value request.
    RENDEZVOUS_SEARCH, ///< A Rendezvous search request.
    START_CONNECTION   ///< A Start connection request.
  } Type;

protected:
  /** Hidden constructor. */
  Request(Type type);

public:
  /** Returns the request type. */
  inline Type type() const { return _type; }
  /** Returns the request cookie. */
  inline const Identifier &cookie() const { return _cookie; }
  /** Returns the timestamp of the request. */
  inline const QDateTime &timestamp() const { return _timestamp; }
  /** Returns @c true if the request is older than the specified number of seconds. */
  inline size_t olderThan(size_t seconds) const {
    return (_timestamp.addMSecs(seconds) < QDateTime::currentDateTime());
  }

protected:
  /** The request type. */
  Type _type;
  /** The magic cookie. */
  Identifier  _cookie;
  /** The request timestamp. */
  QDateTime   _timestamp;
};


/** A ping request item.
 * @ingroup internal */
class PingRequest: public Request
{
public:
  /** Anonymous ping request. */
  PingRequest();
  /** Named ping request. */
  PingRequest(const Identifier &id);
  /** Returns the identifier of the node if this is a named ping request. */
  inline const Identifier &id() const { return _id; }

protected:
  /** Identifier of the node if this is a named ping request. */
  Identifier _id;
};


/** Represents all search requests.
 * @ingroup internal */
class SearchRequest: public Request
{
protected:
  /** Hidden constructor. */
  SearchRequest(Request::Type type, SearchQuery *query);

public:
  /** Returns the query instance associated with the request. */
  inline SearchQuery *query() const { return _query; }

protected:
  /** The search query associated with the request. */
  SearchQuery *_query;
};


/** A find node request item.
 * @ingroup internal */
class FindNodeRequest: public SearchRequest
{
public:
  /** Constructor.
   * @param query Specifies the query object associated with this request. */
  FindNodeRequest(SearchQuery *query);
};


/** A find neighbours request item.
 * @ingroup internal */
class FindNeighboursRequest: public SearchRequest
{
public:
  /** Constructor.
   * @param query Specifies the query object associated with this request. */
  FindNeighboursRequest(SearchQuery *query);
};

/** A find value request item.
 * @ingroup internal */
class FindValueRequest: public SearchRequest
{
public:
  /** Constructor.
   * @param query Specifies the query object associated with this request. */
  FindValueRequest(ValueSearchQuery *query);
};

/** An announce request item.
 * @ingroup internal */
class AnnounceRequest: public SearchRequest
{
public:
  /** Constructor.
   * @param query Specifies the query object associated with this request. */
  AnnounceRequest(SearchQuery *query);
};

/** A Rendezvous search request.
 * Works like a @c FindNeighboursRequest but sends a rendezvous request to every node that appears
 * to know the target node.
 * @ingroup internal */
class RendezvousSearchRequest: public SearchRequest
{
public:
  /** Constructor.
   * @param query Specifies the query object associated with this request. */
  RendezvousSearchRequest(SearchQuery *query);
};


/** A start secure connection request item.
 * @ingroup internal */
class StartConnectionRequest: public Request
{
public:
  /** Constructor.
   * @param service The service identifier.
   * @param peer Identifier of the peer node.
   * @param socket The secure socket for the connection. */
  StartConnectionRequest(const Identifier &service, const Identifier &peer, SecureSocket *socket);

  /** Returns the socket of the request. */
  inline SecureSocket *socket() const { return _socket; }
  /** Returns the service number of the request. */
  inline const char *service() const { return _service; }
  /** Returns the identifier of the remote node. */
  inline const Identifier &peedId() const { return _peer; }

protected:
  /** The service number. */
  Identifier _service;
  /** The id of the remote node. */
  Identifier _peer;
  /** The socket of the connection. */
  SecureSocket *_socket;
};



/* ******************************************************************************************** *
 * Implementation of SearchQuery etc.
 * ******************************************************************************************** */
SearchQuery::SearchQuery(const Identifier &id)
  : _id(id), _best(), _queried()
{
  // Pass...
}

SearchQuery::~SearchQuery() {
  // pass...
}

void
SearchQuery::ignore(const Identifier &id) {
  _queried.insert(id);
}

const Identifier &
SearchQuery::id() const {
  return _id;
}

void
SearchQuery::update(const NodeItem &node) {
  // Skip nodes already queried or in the best list -> done
  if (_queried.contains(node.id())) { return; }
  // Perform an "insort" into best list
  Distance d = _id-node.id();
  QList<NodeItem>::iterator item = _best.begin();
  while ((item != _best.end()) && (d>=(_id-item->id()))) {
    // if the node is in list -> quit
    if (item->id() == node.id()) { return; }
    // continue
    item++;
  }
  _best.insert(item, node);
  while (_best.size() > OVL_K) { _best.pop_back(); }
}

bool
SearchQuery::next(NodeItem &node) {
  QList<NodeItem>::iterator item = _best.begin();
  for (; item != _best.end(); item++) {
    if (! _queried.contains(item->id())) {
      _queried.insert(item->id());
      node = *item;
      return true;
    }
  }
  return false;
}

QList<NodeItem> &
SearchQuery::best() {
  return _best;
}

const QList<NodeItem> &
SearchQuery::best() const {
  return _best;
}

const NodeItem &
SearchQuery::first() const {
  return _best.first();
}

void
SearchQuery::failed() {
  delete this;
}

void
SearchQuery::succeeded() {
  delete this;
}


/* ******************************************************************************************** *
 * Implementation of ValueSearchQuery
 * ******************************************************************************************** */
ValueSearchQuery::ValueSearchQuery(const Identifier &id)
  : SearchQuery(id), _peers()
{
  // pass...
}

ValueSearchQuery::~ValueSearchQuery() {
  // pass...
}

void
ValueSearchQuery::addPeer(const PeerItem &peer) {
  _peers.push_back(peer);
}

const QList<PeerItem> &
ValueSearchQuery::peers() const {
  return _peers;
}

QList<PeerItem> &
ValueSearchQuery::peers() {
  return _peers;
}


/* ******************************************************************************************** *
 * Implementation of Request etc.
 * ******************************************************************************************** */
Request::Request(Type type)
  : _type(type), _cookie(Identifier::create()), _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

PingRequest::PingRequest()
  : Request(PING), _id()
{
  // pass...
}

PingRequest::PingRequest(const Identifier &id)
  : Request(PING), _id(id)
{
  // pass...
}

SearchRequest::SearchRequest(Request::Type type, SearchQuery *query)
  : Request(type), _query(query)
{
  // pass...
}

FindNodeRequest::FindNodeRequest(SearchQuery *query)
  : SearchRequest(FIND_NODE, query)
{
  // pass...
}

FindNeighboursRequest::FindNeighboursRequest(SearchQuery *query)
  : SearchRequest(FIND_NEIGHBOURS, query)
{
  // pass...
}

FindValueRequest::FindValueRequest(ValueSearchQuery *query)
  : SearchRequest(FIND_VALUE, query)
{
  // pass...
}

AnnounceRequest::AnnounceRequest(SearchQuery *query)
  : SearchRequest(ANNOUNCEMENT, query)
{
  // pass...
}

RendezvousSearchRequest::RendezvousSearchRequest(SearchQuery *query)
  : SearchRequest(RENDEZVOUS_SEARCH, query)
{
  // pass...
}

StartConnectionRequest::StartConnectionRequest(const Identifier &service, const Identifier &peer, SecureSocket *socket)
  : Request(START_CONNECTION), _service(service), _peer(peer), _socket(socket)
{
  _cookie = socket->id();
}


/* ******************************************************************************************** *
 * Implementation of DHT
 * ******************************************************************************************** */
Node::Node(Identity &id,
         const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _self(id), _socket(), _started(false),
    _bytesReceived(0), _lastBytesReceived(0), _inRate(0),
    _bytesSend(0), _lastBytesSend(0), _outRate(0), _buckets(_self.id()),
    _connections(), _requestTimer(), _nodeTimer(), _rendezvousTimer(), _announceTimer(),
    _statisticsTimer()
{
  logInfo() << "Start node #" << id.id() << " @ " << addr << ":" << port;

  // try to bind socket to address and port
  if (! _socket.bind(addr, port)) {
    logError() << "Cannot bind to " << addr << ":" << port;
    return;
  }

  // Connect to error slot
  connect(&_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
          this,SLOT(_onSocketError(QAbstractSocket::SocketState)));

  // check request timeouts every 500ms
  _requestTimer.setInterval(500);
  _requestTimer.setSingleShot(false);

  // Update statistics every 5 seconds
  _statisticsTimer.setInterval(1000*5);
  _statisticsTimer.setSingleShot(false);

  // Ping rendezvous nodes every 10s
  _rendezvousTimer.setInterval(1000*10);
  _rendezvousTimer.setSingleShot(false);

  // check for dead nodes every minute
  _nodeTimer.setInterval(1000*60);
  _nodeTimer.setSingleShot(false);

  // Check for dead announcements and check for update my announcement items every 3min
  _announceTimer.setInterval(1000*3);
  _announceTimer.setSingleShot(false);

  connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  connect(&_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(_onBytesWritten(qint64)));
  connect(&_requestTimer, SIGNAL(timeout()), this, SLOT(_onCheckRequestTimeout()));
  connect(&_nodeTimer, SIGNAL(timeout()), this, SLOT(_onCheckNodeTimeout()));
  connect(&_rendezvousTimer, SIGNAL(timeout()), this, SLOT(_onPingRendezvousNodes()));
  connect(&_announceTimer, SIGNAL(timeout()), this, SLOT(_onCheckAnnouncements()));
  connect(&_statisticsTimer, SIGNAL(timeout()), this, SLOT(_onUpdateStatistics()));

  _requestTimer.start();
  _nodeTimer.start();
  _rendezvousTimer.start();
  _announceTimer.start();
  _statisticsTimer.start();

  _started = true;
}

Node::~Node() {
  // pass...
}

void
Node::ping(const QString &addr, uint16_t port) {
  QHostInfo info = QHostInfo::fromName(addr);
  foreach (QHostAddress addr, info.addresses()) {
    ping(addr, port);
  }
}

void
Node::ping(const PeerItem &peer) {
  ping(peer.addr(), peer.port());
}

void
Node::ping(const QHostAddress &addr, uint16_t port) {
  // Create ping request
  PingRequest *req = new PingRequest();
  _pendingRequests.insert(req->cookie(), req);
  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  memcpy(msg.payload.ping.id, _self.id().data(), OVL_HASH_SIZE);
  msg.payload.ping.type = Message::PING;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, OVL_COOKIE_SIZE+OVL_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send ping to " << addr << ":" << port;
  }
}

void
Node::ping(const Identifier &id, const QHostAddress &addr, uint16_t port) {
  // Create named ping request
  PingRequest *req = new PingRequest(id);
  _pendingRequests.insert(req->cookie(), req);
  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  memcpy(msg.payload.ping.id, _self.id().data(), OVL_HASH_SIZE);
  msg.payload.ping.type = Message::PING;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, OVL_COOKIE_SIZE+OVL_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send ping to " << addr << ":" << port;
  }
}

void
Node::ping(const NodeItem &node) {
  ping(node.id(), node.addr(), node.port());
}

void
Node::findNode(const Identifier &id) {
  findNode(new SearchQuery(id));
}

void
Node::findNode(SearchQuery *query) {
  // Create a query instance
  //SearchQuery *query = new SearchQuery(id);
  query->ignore(_self.id());
  // Collect DHT_K nearest nodes
  _buckets.getNearest(query->id(), query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << query->id() << ". Buckets empty.";
    query->failed();
  } else {
    sendFindNode(next, query);
  }
}

void
Node::findValue(const Identifier &id) {
  findValue(new ValueSearchQuery(id));
}

void
Node::findValue(ValueSearchQuery *query) {
  query->ignore(_self.id());
  // Collect DHT_K nearest nodes
  _buckets.getNearest(query->id(), query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find value " << query->id() << ". Buckets empty.";
    // Signal failiour
    emit valueNotFound(query->id(), query->best());
    query->failed();
  } else {
    sendFindValue(next, query);
  }
}

void
Node::announce(const Identifier &id) {
  // Create a query instance
  SearchQuery *query = new SearchQuery(id);
  query->ignore(_self.id());
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not announce " << id << ". Buckets empty.";
    // Emmit signal if failiour is not a pending announcement
    query->failed();
  } else {
    // Update item
    _annouceItems.insert(id, QDateTime::currentDateTime());
    sendAnnouncement(next, query);
  }
}

void
Node::remAnnouncement(const Identifier &id) {
  _annouceItems.remove(id);
}

void
Node::rendezvous(const Identifier &id) {
  // Create a query instance
  SearchQuery *query = new SearchQuery(id);
  query->ignore(_self.id());
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << id << ". Buckets empty.";
    // Emmit signal if failiour is not a pending announcement
    emit rendezvousFailed(id);
    query->failed();
  } else {
    sendRendezvousSearch(next, query);
  }
}

void
Node::findNeighbours(const Identifier &id, const QList<NodeItem> &start) {
  findNeighbours(new SearchQuery(id), start);
}

void
Node::findNeighbours(SearchQuery *query, const QList<NodeItem> &start) {
  query->ignore(_self.id());
  // Collect DHT_K nearest nodes
  _buckets.getNearest(query->id(), query->best());
  // Update with start items
  QList<NodeItem>::const_iterator item = start.begin();
  for (; item != start.end(); item++) { query->update(*item); }
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << query->id() << ". Buckets empty.";
    emit neighboursFound(query->id(), query->best());
    query->failed();
  } else {
    sendFindNeighbours(next, query);
  }
}

bool
Node::hasService(const Identifier &service) const {
  return _services.contains(service);
}

bool
Node::hasService(const char *service) const {
  unsigned char hash[OVL_HASH_SIZE];
  OVLHash((const unsigned char *)service, strlen(service), hash);
  return hasService(Identifier((const char *)hash));
}

bool
Node::registerService(const QString &service, AbstractService *handler) {
  unsigned char hash[OVL_HASH_SIZE];
  QByteArray sName = service.toUtf8();
  OVLHash((const uint8_t *)sName.constData(), sName.size(), hash);
  Identifier id((const char *) hash);
  if (_services.contains(id)) { return false; }
  _services.insert(id, handler);
  return true;
}

bool
Node::startConnection(const QString &service, const NodeItem &node, SecureSocket *stream)
{
  logDebug() << "Send start secure connection id=" << stream->id()
             << " to " << node.id()
             << " @" << node.addr() << ":" << node.port();

  uint8_t serviceId[OVL_HASH_SIZE];
  QByteArray sName = service.toUtf8();
  OVLHash((const uint8_t *)sName.constData(), sName.size(), serviceId);

  StartConnectionRequest *req = new StartConnectionRequest(Identifier((const char *)serviceId), node.id(), stream);

  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  msg.payload.start_connection.type = Message::CONNECT;
  // Store service ID in package
  memcpy(msg.payload.start_connection.service, serviceId, OVL_HASH_SIZE);

  int keyLen = 0;
  if (0 > (keyLen = stream->prepare(msg.payload.start_connection.pubkey, OVL_MAX_PUBKEY_SIZE)) ) {
    stream->failed();
    delete req;
    return false;
  }

  // Compute total size
  keyLen += OVL_COOKIE_SIZE + 1 + OVL_HASH_SIZE;

  // add to pending request list & send it
  _pendingRequests.insert(req->cookie(), req);
  if (keyLen != _socket.writeDatagram((char *)&msg, keyLen, node.addr(), node.port())) {
    // one error remove from list of pending request and free connection & request
    _pendingRequests.remove(req->cookie());
    stream->failed();
    delete req;
    return false;
  }

  return true;
}

void
Node::socketClosed(const Identifier &id) {
  logDebug() << "Secure socket " << id << " closed.";
  _connections.remove(id);
}

Identity &
Node::identity() {
  return _self;
}

const Identity &
Node::identity() const {
  return _self;
}

const Identifier &
Node::id() const {
  return _self.id();
}

bool
Node::started() const {
  // Check if socket is bound
  return (_started && _socket.isValid() &&
          (QAbstractSocket::BoundState == _socket.state()));
}

size_t
Node::numNodes() const {
  return _buckets.numNodes();
}

void
Node::nodes(QList<NodeItem> &lst) {
  _buckets.nodes(lst);
}

size_t
Node::numSockets() const {
  return _connections.size();
}

size_t
Node::bytesReceived() const {
  return _bytesReceived;
}

size_t
Node::bytesSend() const {
  return _bytesSend;
}

double
Node::inRate() const {
  return _inRate;
}

double
Node::outRate() const {
  return _outRate;
}

bool
Node::rendezvousPingEnabled() const {
  return _rendezvousTimer.isActive();
}

void
Node::enableRendezvousPing(bool enable) {
  if (enable)
    _rendezvousTimer.start();
  else
    _rendezvousTimer.stop();
}


/*
 * Implementation of internal used methods.
 */
void
Node::sendFindNode(const NodeItem &to, SearchQuery *query) {
  /*logDebug() << "Send FindNode request to " << to.id()
             << " @" << to.addr() << ":" << to.port(); */
  // Construct request item
  FindNodeRequest *req = new FindNodeRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  msg.payload.search.type = Message::FIND_NODE;
  memcpy(msg.payload.search.id, query->id().data(), OVL_HASH_SIZE);
  int size = OVL_COOKIE_SIZE+1+OVL_HASH_SIZE+OVL_K*OVL_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindNode request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
Node::sendFindNeighbours(const NodeItem &to, SearchQuery *query) {
  // Construct request item
  FindNeighboursRequest *req = new FindNeighboursRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  msg.payload.search.type = Message::FIND_NODE;
  memcpy(msg.payload.search.id, query->id().data(), OVL_HASH_SIZE);
  int size = OVL_COOKIE_SIZE+1+OVL_HASH_SIZE+OVL_K*OVL_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindNode request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
Node::sendAnnouncement(const NodeItem &to, SearchQuery *query) {
  // Construct request item
  AnnounceRequest *req = new AnnounceRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  msg.payload.search.type = Message::ANNOUNCE;
  memcpy(msg.payload.search.id, query->id().data(), OVL_HASH_SIZE);
  int size = OVL_COOKIE_SIZE+1+OVL_HASH_SIZE+OVL_K*OVL_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send Announce request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
Node::sendFindValue(const NodeItem &to, ValueSearchQuery *query) {
  // Construct request item
  FindValueRequest *req = new FindValueRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  msg.payload.search.type = Message::FIND_VALUE;
  memcpy(msg.payload.search.id, query->id().data(), OVL_HASH_SIZE);
  int size = OVL_COOKIE_SIZE+1+OVL_HASH_SIZE+OVL_K*OVL_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindValue request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
Node::sendRendezvousSearch(const NodeItem &to, SearchQuery *query) {
  // Construct request item
  RendezvousSearchRequest *req = new RendezvousSearchRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  msg.payload.search.type = Message::FIND_NODE;
  memcpy(msg.payload.search.id, query->id().data(), OVL_HASH_SIZE);
  int size = OVL_COOKIE_SIZE+1+OVL_HASH_SIZE+OVL_K*OVL_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send Rendezvous request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
Node::sendRendezvous(const Identifier &with, const PeerItem &to) {
  Message msg;
  memcpy(msg.cookie, Identifier::create().data(), OVL_COOKIE_SIZE);
  memcpy(msg.payload.rendezvous.id, with.data(), OVL_HASH_SIZE);
  if (OVL_RENDEZVOUS_REQU_SIZE != _socket.writeDatagram(
        (char *)&msg, OVL_RENDEZVOUS_REQU_SIZE, to.addr(), to.port())) {
    logError() << "DHT: Failed to send Rendezvous request to " << to.addr() << ":" << to.port();
  }
}

bool
Node::sendData(const Identifier &id, const uint8_t *data, size_t len, const PeerItem &peer) {
  return sendData(id, data, len, peer.addr(), peer.port());
}

bool
Node::sendData(const Identifier &id, const uint8_t *data, size_t len, const QHostAddress &addr, uint16_t port) {
  if (len > OVL_MAX_DATA_SIZE) {
    logError() << "DHT: sendData(): Cannot send connection data: payload too large "
               << len << ">" << OVL_MAX_DATA_SIZE << "!";
    return false;
  }
  // Assemble message
  Message msg;
  memcpy(msg.cookie, id.constData(), OVL_COOKIE_SIZE);
  memcpy(msg.payload.datagram, data, len);
  // send it
  return (qint64(len+OVL_COOKIE_SIZE) ==
          _socket.writeDatagram((const char *)&msg, (len+OVL_COOKIE_SIZE), addr, port));
}

void
Node::_onReadyRead() {
  while (_socket.hasPendingDatagrams()) {
    // check datagram size
    if ( (_socket.pendingDatagramSize() > OVL_MAX_MESSAGE_SIZE) ||
         (_socket.pendingDatagramSize() < OVL_MIN_MESSAGE_SIZE)) {
      QHostAddress addr; uint16_t port;
      // Cannot be a vaild message -> drop it
      _socket.readDatagram(0, 0, &addr, &port);
      logInfo() << "Invalid UDP packet received from " << addr << ":" << port;
    } else {
      // Update statistics
      _bytesReceived += _socket.pendingDatagramSize();
      // Read message
      struct Message msg; QHostAddress addr; uint16_t port;
      int64_t size = _socket.readDatagram((char *) &msg, sizeof(Message), &addr, &port);
      // Extract cookie
      Identifier cookie(msg.cookie);

      // First, check if message belongs to a open stream
      if (_connections.contains(cookie)) {
        // Process streams
        _connections[cookie]->handleData(((uint8_t *)&msg)+OVL_COOKIE_SIZE, size-OVL_COOKIE_SIZE);
      } else if (_pendingRequests.contains(cookie)) {
        // Message is a response -> dispatch by type from table
        Request *item = _pendingRequests[cookie];
        // remove from pending requests
        _pendingRequests.remove(cookie);
        if (Request::PING == item->type()) {
          _processPingResponse(msg, size, static_cast<PingRequest *>(item), addr, port);
        } else if (Request::FIND_NODE == item->type()) {
          _processFindNodeResponse(msg, size, static_cast<FindNodeRequest *>(item), addr, port);
        } else if (Request::FIND_NEIGHBOURS == item->type()) {
          _processFindNeighboursResponse(msg, size, static_cast<FindNeighboursRequest *>(item), addr, port);
        } else if (Request::RENDEZVOUS_SEARCH == item->type()) {
          _processRendezvousSearchResponse(msg, size, static_cast<RendezvousSearchRequest *>(item), addr, port);
        } else if (Request::START_CONNECTION == item->type()) {
          _processStartConnectionResponse(msg, size, static_cast<StartConnectionRequest *>(item), addr, port);
        }else {
          logInfo() << "Unknown response from " << addr << ":" << port;
        }
        delete item;
      } else {
        // Message is likely a request
        if ((size == OVL_PING_REQU_SIZE) && (Message::PING == msg.payload.ping.type)){
          _processPingRequest(msg, size, addr, port);
        } else if ((size >= OVL_SEARCH_MIN_REQU_SIZE) && (Message::FIND_NODE == msg.payload.search.type)) {
          _processFindNodeRequest(msg, size, addr, port);
        } else if ((size > OVL_CONNECT_MIN_REQU_SIZE) && (Message::CONNECT == msg.payload.start_connection.type)) {
          _processStartConnectionRequest(msg, size, addr, port);
        } else if ((size == OVL_RENDEZVOUS_REQU_SIZE) && (Message::RENDEZVOUS == msg.payload.rendezvous.type)) {
          _processRendezvousRequest(msg, size, addr, port);
        } else {
          logInfo() << "Unknown request from " << addr << ":" << port
                    << " dropping " << (size-OVL_COOKIE_SIZE) << "b payload.";
        }
      }
    }
  }
}

void
Node::_onBytesWritten(qint64 n) {
  _bytesSend += n;
}

void
Node::_onSocketError(QAbstractSocket::SocketState error) {
  logError() << "DHT: Socket state changed: " << _socket.state();
}

void
Node::_processPingResponse(
    const struct Message &msg, size_t size, PingRequest *req, const QHostAddress &addr, uint16_t port)
{
  // signal success
  emit nodeReachable(NodeItem(Identifier(msg.payload.ping.id), addr, port));
  // If the buckets are empty -> we are likely bootstrapping
  bool bootstrapping = _buckets.empty();
  // Given that this is a ping response -> add the node to the corresponding
  // bucket if space is left
  if (_buckets.add(Identifier(msg.payload.ping.id), addr, port)) {
    emit nodeAppeard(NodeItem(Identifier(msg.payload.ping.id), addr, port));
  }
  if (bootstrapping) {
    emit connected();
    logDebug() << "Still boot strapping: Search for myself.";
    findNode(_self.id());
  }
}

void
Node::_processFindNodeResponse(
    const struct Message &msg, size_t size, FindNodeRequest *req, const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 == ((size-OVL_SEARCH_MIN_RESP_SIZE)%OVL_TRIPLE_SIZE) ) {
    // unpack and update query
    size_t Ntriple = (size-OVL_SEARCH_MIN_RESP_SIZE)/OVL_TRIPLE_SIZE;
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      NodeItem item(id, QHostAddress((const Q_IPV6ADDR &)*(msg.payload.result.triples[i].ip)),
                    ntohs(msg.payload.result.triples[i].port));
      // Add discovered node to buckets
      _buckets.addCandidate(id, item.addr(), item.port());
      // Update node list of query
      req->query()->update(item);
    }

    // If the node was found -> signal success
    if (req->query()->best().size() && (req->query()->id() == req->query()->first().id())) {
      // Signal node found
      emit nodeFound(req->query()->first());
      // signel query succeeded
      req->query()->succeeded();
      // done
      return;
    }
  } else {
    logInfo() << "Received a malformed FIND_NODE response from "
              << addr << ":" << port;
  }
  // Get next node to query
  NodeItem next;
  // get next node to query, if there is no next node -> search failed
  if (! req->query()->next(next)) {
    // If the node search is a pending announcement
    logInfo() << "Node " << req->query()->id() << " not found.";
    // if it was a regular node search -> signal error
    emit nodeNotFound(req->query()->id(), req->query()->best());
    // delete query
    req->query()->failed();
    // done.
    return;
  }

  // Send next request
  sendFindNode(next, req->query());
}

void
Node::_processFindValueResponse(
    const struct Message &msg, size_t size, FindValueRequest *req, const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 == ((size-OVL_SEARCH_MIN_RESP_SIZE)%OVL_TRIPLE_SIZE) ) {
    // unpack and update query
    size_t Ntriple = (size-OVL_SEARCH_MIN_RESP_SIZE)/OVL_TRIPLE_SIZE;
    if (msg.payload.result.success) {
      QList<NodeItem> nodes;
      // On value found -> unpack result and signal success
      for (size_t i=0; i<Ntriple; i++) {
        Identifier id(msg.payload.result.triples[i].id);
        QHostAddress addr((const Q_IPV6ADDR &)*(msg.payload.result.triples[i].ip));
        uint16_t port = ntohs(msg.payload.result.triples[i].port);
        static_cast<ValueSearchQuery *>(req->query())->addPeer(PeerItem(addr, port));
        nodes.push_back(NodeItem(id, addr, port));
      }
      // Signal node found
      emit valueFound(req->query()->id(), nodes);
      // delete query
      req->query()->succeeded();
      // done
      return;
    } else {
      // If value not found update next nodes and continue search
      for (size_t i=0; i<Ntriple; i++) {
        Identifier id(msg.payload.result.triples[i].id);
        NodeItem item(id, QHostAddress((const Q_IPV6ADDR &)*(msg.payload.result.triples[i].ip)),
                      ntohs(msg.payload.result.triples[i].port));
        // Add discovered node to buckets
        _buckets.addCandidate(id, item.addr(), item.port());
        // Update node list of query
        req->query()->update(item);
      }
    }
  } else {
    logInfo() << "Received a malformed FIND_VALUE response from "
              << addr << ":" << port;
  }
  // Get next node to query
  NodeItem next;
  // get next node to query, if there is no next node -> search failed
  if (! req->query()->next(next)) {
    // If the node search is a pending announcement
    logInfo() << "Value " << req->query()->id() << " not found.";
    // if it was a regular node search -> signal error
    emit valueNotFound(req->query()->id(), req->query()->best());
    // delete query
    req->query()->failed();
    // done.
    return;
  }

  // Send next request
  sendFindValue(next, reinterpret_cast<ValueSearchQuery *>(req->query()));
}

void
Node::_processAnnounceResponse(
    const struct Message &msg, size_t size, AnnounceRequest *req, const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 == ((size-OVL_SEARCH_MIN_RESP_SIZE)%OVL_TRIPLE_SIZE) ) {
    // unpack and update query
    size_t Ntriple = (size-OVL_SEARCH_MIN_RESP_SIZE)/OVL_TRIPLE_SIZE;
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      NodeItem item(id, QHostAddress((const Q_IPV6ADDR &)*(msg.payload.result.triples[i].ip)),
                    ntohs(msg.payload.result.triples[i].port));
      // Add discovered node to buckets
      _buckets.addCandidate(id, item.addr(), item.port());
      // Update node list of query
      req->query()->update(item);
    }
  } else {
    logInfo() << "Received a malformed ANNOUNCE response from "
              << addr << ":" << port;
  }
  // Get next node to query
  NodeItem next;
  // get next node to query, if there is no next node -> search done
  if (! req->query()->next(next)) {
    // If the node search is a pending announcement
    logInfo() << "Announcement of " << req->query()->id() << " completed.";
    // delete query
    req->query()->succeeded();
    // done.
    return;
  }

  // Send next request
  sendAnnouncement(next, req->query());
}

void
Node::_processFindNeighboursResponse(const Message &msg, size_t size, FindNeighboursRequest *req,
                                    const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 != ((size-OVL_SEARCH_MIN_RESP_SIZE)%OVL_TRIPLE_SIZE) ) {
    logInfo() << "Received a malformed FIND_NODE response from "
              << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-OVL_SEARCH_MIN_RESP_SIZE)/OVL_TRIPLE_SIZE;
    // proceed with returned nodes
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      NodeItem item(id, QHostAddress((const Q_IPV6ADDR &)* (msg.payload.result.triples[i].ip)),
                    ntohs(msg.payload.result.triples[i].port));
      // Add discovered node to buckets
      _buckets.addCandidate(id, item.addr(), item.port());
      // Update node list of query
      req->query()->update(item);
    }
  }

  NodeItem next;
  // get next node to query, if there is no next node -> search failed
  if (! req->query()->next(next)) {
    // signal error
    emit neighboursFound(req->query()->id(), req->query()->best());
    // delete query
    req->query()->succeeded();
    // done.
    return;
  }
  // Send next request
  sendFindNeighbours(next, req->query());
}

void
Node::_processRendezvousSearchResponse(const Message &msg, size_t size, RendezvousSearchRequest *req,
                                      const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 != ((size-OVL_SEARCH_MIN_RESP_SIZE)%OVL_TRIPLE_SIZE) ) {
    logInfo() << "Received a malformed FIND_NODE response from "
              << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-OVL_SEARCH_MIN_RESP_SIZE)/OVL_TRIPLE_SIZE;
    // proceed with returned nodes
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      QHostAddress nodeaddr((const Q_IPV6ADDR &)* (msg.payload.result.triples[i].ip));
      uint16_t     nodeport(ntohs(msg.payload.result.triples[i].port));
      // Add discovered node to buckets
      _buckets.addCandidate(id, nodeaddr, nodeport);
      // Update node list of query
      req->query()->update(NodeItem(id, nodeaddr, nodeport));
      // If the peer knows the target -> send him a rendezvous message
      if (req->query()->id() == id) {
        sendRendezvous(req->query()->id(), PeerItem(addr, port));
      }
    }
  }

  NodeItem next;
  // get next node to query, if there is no next node -> search failed
  if (! req->query()->next(next)) {
    // If at least someone I asked knows the item -> success
    if (req->query()->best().size() && (req->query()->first().id() == req->query()->id())) {
      emit rendezvousInitiated(req->query()->first());
    }
    // signal error
    emit rendezvousFailed(req->query()->id());
    // delete query
    req->query()->failed();
    // done.
    return;
  }
  // Send next request
  sendRendezvousSearch(next, req->query());
}

void
Node::_processStartConnectionResponse(
    const Message &msg, size_t size, StartConnectionRequest *req, const QHostAddress &addr, uint16_t port)
{
  // Verify session key
  if (! req->socket()->verify(msg.payload.start_connection.pubkey, size-OVL_COOKIE_SIZE-3)) {
    logError() << "Verification of peer session key failed.";
    req->socket()->failed();
    return;
  }

  // verify fingerprints
  if (!(req->socket()->peerId() == req->peedId())) {
    logError() << "Peer fingerprint mismatch: " << req->socket()->peerId()
               << " != " << req->peedId();
    req->socket()->failed();
    return;
  }

  // success -> start connection
  if (! req->socket()->start(req->cookie(), PeerItem(addr, port))) {
    logError() << "Can not initialize symmetric chipher.";
    req->socket()->failed();
    return;
  }

  // Stream started: register stream
  _connections[req->cookie()] = req->socket();
}

void
Node::_processPingRequest(
    const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  //logDebug() << "Received Ping request from " << addr << ":" << port;
  // simply assemble a pong response including my own ID
  Message resp;
  memcpy(resp.cookie, msg.cookie, OVL_COOKIE_SIZE);
  memcpy(resp.payload.ping.id, _self.id().data(), OVL_HASH_SIZE);
  resp.payload.ping.type = Message::PING;
  // send
  //logDebug() << "Send Ping response to " << addr << ":" << port;
  if(0 > _socket.writeDatagram((char *) &resp, OVL_PING_RESP_SIZE, addr, port)) {
    logError() << "Failed to send Ping response to " << addr << ":" << port;
  }
  // Add node to candidate nodes for the bucket table if not known already
  if (! _buckets.contains(Identifier(msg.payload.ping.id))) {
    _buckets.addCandidate(Identifier(msg.payload.ping.id), addr, port);
  }
}

void
Node::_processFindNodeRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  QList<NodeItem> best;
  _buckets.getNearest(Identifier(msg.payload.search.id), best);

  struct Message resp;
  // Assemble response
  memcpy(resp.cookie, msg.cookie, OVL_COOKIE_SIZE);
  resp.payload.result.success = 0;
  // Determine the number of nodes to reply
  int maxN = int(size-OVL_SEARCH_MIN_REQU_SIZE)/OVL_TRIPLE_SIZE;
  int N = std::min(best.size(), maxN);

  // Add items
  QList<NodeItem>::iterator item = best.begin();
  for (int i = 0; (item!=best.end()) && (i<N); item++, i++) {
    memcpy(resp.payload.result.triples[i].id, item->id().data(), OVL_HASH_SIZE);
    memcpy(resp.payload.result.triples[i].ip, item->addr().toIPv6Address().c, 16);
    resp.payload.result.triples[i].port = htons(item->port());
  }

  // Compute size and send reponse
  size_t resp_size = (OVL_SEARCH_MIN_RESP_SIZE + N*OVL_TRIPLE_SIZE);
  _socket.writeDatagram((char *) &resp, resp_size, addr, port);
}

void
Node::_processFindValueRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  struct Message resp;
  // Assemble response
  memcpy(resp.cookie, msg.cookie, OVL_COOKIE_SIZE);
  Identifier queryId(msg.payload.search.id);
  int maxN = int(size-OVL_SEARCH_MIN_REQU_SIZE)/OVL_TRIPLE_SIZE;
  QList<NodeItem> res;

  if (_hashTable.contains(queryId)) {
    // On success (I hold a list of responsible nodes)
    resp.payload.result.success = 1;
    // Determine the number of nodes to reply
    int N = std::min(std::min(OVL_K, _hashTable[queryId].size()), maxN);
    QSet<AnnouncementItem>::iterator node= _hashTable[queryId].begin();
    for (int i=0; i<N; i++, node++) {
      res.append(NodeItem(Identifier(), *node));
    }
  } else {
    resp.payload.result.success = 0;
    QList<NodeItem> best;
    _buckets.getNearest(Identifier(msg.payload.search.id), best);
    // Determine the number of nodes to reply
    int N = std::min(std::min(OVL_K, best.size()), maxN);
    QList<NodeItem>::iterator item = best.begin();
    for (int i = 0; (i<N); item++, i++) {
      res.append(*item);
    }
  }

  // Add items
  QList<NodeItem>::iterator item = res.begin();
  for (int i = 0; item!=res.end(); item++, i++) {
    memcpy(resp.payload.result.triples[i].id, item->id().data(), OVL_HASH_SIZE);
    memcpy(resp.payload.result.triples[i].ip, item->addr().toIPv6Address().c, 16);
    resp.payload.result.triples[i].port = htons(item->port());
  }

  // Compute size and send reponse
  size_t resp_size = (OVL_SEARCH_MIN_RESP_SIZE + res.size()*OVL_TRIPLE_SIZE);
  _socket.writeDatagram((char *) &resp, resp_size, addr, port);
}

void
Node::_processAnnounceRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  // Assemble response
  struct Message resp;
  memcpy(resp.cookie, msg.cookie, OVL_COOKIE_SIZE);
  resp.payload.result.success = 0;

  Identifier queryId(msg.payload.search.id);
  int maxN = int(size-OVL_SEARCH_MIN_REQU_SIZE)/OVL_TRIPLE_SIZE;

  // Determine the number of nodes to reply
  QList<NodeItem> best;
  _buckets.getNearest(queryId, best);
  int N = std::min(maxN, best.size());

  QList<NodeItem>::iterator item = best.begin();
  for (int i = 0; i<N; item++, i++) {
    memcpy(resp.payload.result.triples[i].id, item->id().data(), OVL_HASH_SIZE);
    memcpy(resp.payload.result.triples[i].ip, item->addr().toIPv6Address().c, 16);
    resp.payload.result.triples[i].port = htons(item->port());
  }

  // Compute size and send reponse
  size_t resp_size = (OVL_SEARCH_MIN_RESP_SIZE + N*OVL_TRIPLE_SIZE);
  _socket.writeDatagram((char *) &resp, resp_size, addr, port);

  // If I am responsible for that value -> add to
  Distance my_d = _self.id()-queryId;
  for (item=best.begin(); item!=best.end(); item++) {
    // If I am closer to the announced data than one of the K nearest nodes I know,
    // -- that is the value is in my neightbourhood -- I take it.
    if (my_d < (item->id()-queryId)) {
      if (! _hashTable.contains(queryId)) {
        _hashTable.insert(queryId, QSet<AnnouncementItem>());
      }
      // insert or replace annoucement
      _hashTable[queryId].insert(AnnouncementItem(addr, port));
      return;
    }
  }
}

void
Node::_processStartConnectionRequest(const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  Identifier service((const char *)msg.payload.start_connection.service);
  logDebug() << "Received StartConnection request, service: " << service;
  if (! _services.contains(service)) { return; }
  AbstractService *serviceHandler = _services[service];

  // request new connection from service handler
  SecureSocket *connection = 0;
  if (0 == (connection = serviceHandler->newSocket()) ) {
    logInfo() << "Connection handler refuses to create a new connection."; return;
  }

  // verify request
  if (! connection->verify(msg.payload.start_connection.pubkey, size-OVL_COOKIE_SIZE-3)) {
    logError() << "Can not verify connection peer.";
    delete connection; return;
  }

  // check if connection is allowed
  if (! serviceHandler->allowConnection(NodeItem(connection->peerId(), addr, port))) {
    logInfo() << "Connection recjected by Service.";
    delete connection; return;
  }

  // assemble response
  Message resp; int keyLen=0;
  memcpy(resp.cookie, msg.cookie, OVL_COOKIE_SIZE);
  resp.payload.start_connection.type = Message::CONNECT;
  memcpy(resp.payload.start_connection.service,
         msg.payload.start_connection.service, OVL_HASH_SIZE);
  if (0 > (keyLen = connection->prepare(resp.payload.start_connection.pubkey, OVL_MAX_PUBKEY_SIZE))) {
    logError() << "Can not prepare connection.";
    delete connection; return;
  }

  if (! connection->start(Identifier(resp.cookie), PeerItem(addr, port))) {
    logError() << "Can not finish SecureSocket handshake.";
    delete connection; return;
  }

  // compute message size
  keyLen += OVL_CONNECT_MIN_RESP_SIZE;
  // Send response
  if (keyLen != _socket.writeDatagram((char *)&resp, keyLen, addr, port)) {
    logError() << "Can not send StartConnection response";
    delete connection; return;
  }

  // Connection started..
  _connections[Identifier(resp.cookie)] = connection;
  serviceHandler->connectionStarted(connection);
}

void
Node::_processRendezvousRequest(Message &msg, size_t size, const QHostAddress &addr, uint16_t port) {
  if (_self.id() == Identifier(msg.payload.rendezvous.id)) {
    // If the rendezvous request addressed me -> response with a ping
    ping(msg.payload.rendezvous.ip, ntohs(msg.payload.rendezvous.port));
  } else if (_buckets.contains(Identifier(msg.payload.rendezvous.id))) {
    // If the rendezvous request is not addressed to me but to a node I know -> forward
    NodeItem node = _buckets.getNode(Identifier(msg.payload.rendezvous.id));
    memcpy(msg.payload.rendezvous.ip, addr.toIPv6Address().c, 16);
    msg.payload.rendezvous.port = htons(port);
    if (OVL_RENDEZVOUS_REQU_SIZE != _socket.writeDatagram(
          (char *)&msg, OVL_RENDEZVOUS_REQU_SIZE, node.addr(), node.port())) {
      logError() << "DHT: Cannot forward rendezvous request to " << node.id()
                 << " @" << node.addr() << ":" << node.port();
    }  
  }
  // silently ignore rendezvous requests to an unknown node.
}

void
Node::_onCheckRequestTimeout() {
  // Remove dead requests from pending request list
  QList<Request *> deadRequests;
  QHash<Identifier, Request *>::iterator item = _pendingRequests.begin();
  while (item != _pendingRequests.end()) {
    if ((*item)->olderThan(2000)) {
      deadRequests.append(*item);
      item = _pendingRequests.erase(item);
    } else {
      item++;
    }
  }

  // Process dead requests, dispatch by type
  QList<Request *>::iterator req = deadRequests.begin();
  for (; req != deadRequests.end(); req++) {
    if (Request::PING == (*req)->type()) {
      logDebug() << "Ping request timeout...";
      PingRequest *ping = static_cast<PingRequest *>(*req);
      // If the ping was send to a known node (known ID, and not to a peer)
      if (ping->id().isValid()) {
        _buckets.pingLost(ping->id());
      }
      delete ping;
    } else if (Request::FIND_NODE == (*req)->type()) {
      logDebug() << "FindNode request timeout...";
      SearchQuery *query = static_cast<FindNodeRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> search failed
      if (! query->next(next)) {
        // if it was a regular node search -> signal error
        emit nodeNotFound(query->id(), query->best());
        query->failed();
        // delete request and query
        delete *req;
      } else {
        // Continue search
        sendFindNode(next, query); delete *req;
      }
    } else if (Request::FIND_VALUE == (*req)->type()) {
      logDebug() << "FindValue request timeout...";
      ValueSearchQuery *query = static_cast<ValueSearchQuery *>(
            static_cast<FindValueRequest *>(*req)->query());
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> search failed
      if (! query->next(next)) {
        // if it was a regular node search -> signal error
        emit valueNotFound(query->id(), query->best());
        query->failed();
        // delete request
        delete *req;
      } else {
        // Continue search
        sendFindValue(next, query);
        delete *req;
      }
    } else if (Request::ANNOUNCEMENT == (*req)->type()) {
      logDebug() << "Announcement request timeout...";
      SearchQuery *query = static_cast<AnnounceRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> search failed
      if (! query->next(next)) {
        // Silently ignore...
        query->succeeded();
        // delete request
        delete *req;
      } else {
        // Continue search
        sendAnnouncement(next, query);
        delete *req;
      }
    } else if (Request::FIND_NEIGHBOURS == (*req)->type()) {
      logDebug() << "FindNeighbours request timeout...";
      SearchQuery *query = static_cast<FindNeighboursRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> done
      if (! query->next(next)) {
        // signal end of search
        emit neighboursFound(query->id(), query->best());
        query->succeeded();
        // delete request & query
        delete *req;
      } else {
        // Send next request
        sendFindNeighbours(next, query);
        delete *req;
      }
    } else if (Request::RENDEZVOUS_SEARCH == (*req)->type()) {
      logDebug() << "RendezvousSearch request timeout...";
      SearchQuery *query = static_cast<RendezvousSearchRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> done
      if (! query->next(next)) {
        // If node was found -> rendezvous initiated
        if (query->best().size() && (query->first().id() == query->id())) {
          emit rendezvousInitiated(query->first());
          query->succeeded();
        } else {
          // if not -> failed.
          emit rendezvousFailed(query->id());
          query->failed();
        }
        // delete request
        delete *req;
      } else {
        // Send next request
        sendRendezvousSearch(next, query);
        delete *req;
      }
    } else if (Request::START_CONNECTION == (*req)->type()) {
      logDebug() << "StartConnection request timeout...";
      // signal timeout
      static_cast<StartConnectionRequest *>(*req)->socket()->failed();
      // delete request
      delete *req;
    }
  }
}

void
Node::_onCheckNodeTimeout() {
  // Collect nodes older than 15min from the buckets
  QList<NodeItem> oldNodes;
  _buckets.getOlderThan(15*60, oldNodes);
  // send a ping to all of them
  QList<NodeItem>::iterator node = oldNodes.begin();
  for (; node != oldNodes.end(); node++) {
    ping(node->addr(), node->port());
  }

  // Get disappeared nodes
  oldNodes.clear(); _buckets.getOlderThan(20*60, oldNodes);
  for (node = oldNodes.begin(); node != oldNodes.end(); node++) {
    emit nodeLost(node->id());
  }
  // are buckets non-empty (this node is connected to the network)
  bool connected = (0 != _buckets.numNodes());
  // Remove dead nodes from the buckets
  _buckets.removeOlderThan(20*60);
  // check if the last node was removed
  if (connected && (0 == _buckets.numNodes())) {
    // If the last node was removed -> signal connection loss
    emit disconnected();
  }

  // Update neighbourhood
  if (_buckets.numNodes()) {
    // search for myself, this will certainly fail but results in a list
    // of the closest nodes, which will be added to the buckets as candidates
    findNeighbours(_self.id());
  }
}

void
Node::_onPingRendezvousNodes() {
  QList<NodeItem> nodes;
  _buckets.getNearest(id(), nodes);
  QList<NodeItem>::iterator node = nodes.begin();
  for (; node != nodes.end(); node++) {
    ping(*node);
  }
}

void
Node::_onCheckAnnouncements() {
  // Remove all announcement older than an hour:
  QHash<Identifier, QSet<AnnouncementItem> >::iterator item = _hashTable.begin();
  for (; item != _hashTable.end(); item++) {
    QSet<AnnouncementItem>::iterator peer = item->begin();
    while(peer != item->end()) {
      if (peer->olderThan(60*60)) {
        peer = item->erase(peer);
      } else {
        peer++;
      }
    }
  }

  // Reannounce all item older than 25min
  QHash<Identifier, QDateTime>::iterator aitem = _annouceItems.begin();
  for (; aitem != _annouceItems.end(); aitem++) {
    if (aitem->secsTo(QDateTime::currentDateTime())>=25*60) {
      announce(aitem.key());
    }
  }
}

void
Node::_onUpdateStatistics() {
  _inRate = (double(_bytesReceived - _lastBytesReceived)/_statisticsTimer.interval())/1000;
  _lastBytesReceived = _bytesReceived;
  _outRate = (double(_bytesSend - _lastBytesSend)/_statisticsTimer.interval())/1000;
  _lastBytesSend = _bytesSend;
}
