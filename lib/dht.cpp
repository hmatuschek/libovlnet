#include "dht.h"
#include "crypto.h"
#include "dht_config.h"

#include <QHostInfo>
#include <netinet/in.h>
#include <inttypes.h>


/** Represents a triple of ID, IP address and port as transferred via UDP.
 * @ingroup internal */
struct __attribute__((packed)) DHTTriple {
  /** The ID of a node. */
  char      id[DHT_HASH_SIZE];
  /** The IP of the node. */
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
    /** An announce message. */
    ANNOUNCE,
    /** A find node request message. */
    FIND_NODE,
    /** A find value request message. */
    FIND_VALUE,
    /** A "connect" request or response message. */
    START_STREAM,
    /** A rendezvous request or notification message. */
    RENDEZVOUS,
  } Type;

  /** The magic cookie to match a response to a request. */
  char cookie[DHT_COOKIE_SIZE];

  /** Payload. */
  union __attribute__((packed)) {
    /** A ping message (request & response) consists of a type identifier
     * and the ID of the sender. */
    struct __attribute__((packed)){
      uint8_t type;               ///< Type flag == @c MSG_PING.
      char    id[DHT_HASH_SIZE];  ///< Identifier of the sender
    } ping;

    /** An announcement message. */
    struct __attribute__((packed)) {
      uint8_t type;                ///< Type flag == @c MSG_ANNOUNCE.
      char    who[DHT_HASH_SIZE];  ///< Identifier of the sender.
      char    what[DHT_HASH_SIZE]; ///< Identifier of the object.
    } announce;

    /** A find node request. */
    struct __attribute__((packed)) {
      /** Type flag == @c MSG_FIND_NODE. */
      uint8_t type;
      /** The identifier of the node to find. */
      char    id[DHT_HASH_SIZE];
      /** This dummy payload is needed to avoid the risk to exploid this request for a relay DoS
       * attack. It ensures that the request has at least the same size as the response. The size
       * of this field implicitly defines the number of triples returned by the remote node. */
      char    dummy[DHT_MAX_TRIPLES*DHT_TRIPLE_SIZE-DHT_HASH_SIZE];
    } find_node;

    struct __attribute__((packed)) {
      /** Type flag == @c MSG_FIND_VALUE. */
      uint8_t type;
      /** The identifier of the value to find. */
      char    id[DHT_HASH_SIZE];
      /** This dummy payload is needed to avoid the risk to exploid this request for a relay DoS
       * attack. It ensures that the request has at least the same size as the response. The size
       * of this field implicitly defines the max. number of triples returned by the remote node. */
      char    dummy[DHT_MAX_TRIPLES*DHT_TRIPLE_SIZE-DHT_HASH_SIZE];
    } find_value;

    /** A response to "find value" or "find node". */
    struct __attribute__((packed)) {
      /** If a FIND_VALUE response, this flag indicates success. Then, the triples contain the
       * nodes associated with the requested value. Otherwise the triples contain the nodes to
       * ask next. */
      uint8_t   success;
      /** Node tiples (ID, address, port). */
      DHTTriple triples[DHT_MAX_TRIPLES];
    } result;

    /** A start secure connection request and response. Implementing the ECDH handshake. */
    struct __attribute__((packed)) {
      /** Type flag == @c MSG_START_CONNECTION. */
      uint8_t  type;
      /** A service id (not part of the DHT specification). */
      uint16_t service;
      /** Public (ECDH) key of the requesting or responding node. */
      uint8_t  pubkey[DHT_MAX_PUBKEY_SIZE];
    } start_connection;

    /** A rendesvous request or notification message. */
    struct __attribute__((packed)) {
      /** Type flag == @c MSG_RENDEZVOUS. */
      uint8_t  type;
      /** Specifies the ID of the node to date. */
      char     id[DHT_HASH_SIZE];
      /** Will be set by the rendezvous server to the source address of the reuquest sender, 
       * before relaying it to the target. */
      char     ip[16];
      /** Will be set by the rendezvous server to the source port of the request sender, before 
       * relaying it to the target. */ 
      uint16_t port;
    } rendezvous; 

    /** A stream datagram. */
    uint8_t datagram[DHT_MAX_DATA_SIZE];
  } payload;

  /** Constructor. */
  Message();
};

Message::Message()
{
  memset(this, 0, sizeof(Message));
}


#define DHT_PING_REQU_SIZE             (DHT_COOKIE_SIZE+DHT_HASH_SIZE+1)
#define DHT_PING_RESP_SIZE             DHT_PING_REQU_SIZE
#define DHT_FIND_NODE_MIN_REQU_SIZE    (DHT_COOKIE_SIZE+DHT_HASH_SIZE+1)
#define DHT_FIND_NODE_MIN_RESP_SIZE    (DHT_COOKIE_SIZE+1)
#define DHT_FIND_NEIGHBOR_MIN_REQU_SIZE  (DHT_COOKIE_SIZE+DHT_HASH_SIZE+1)
#define DHT_FIND_NEIGHBOR_MIN_RESP_SIZE  (DHT_COOKIE_SIZE+1)
#define DHT_START_STREAM_MIN_REQU_SIZE (DHT_COOKIE_SIZE+DHT_HASH_SIZE+3)
#define DHT_START_STREAM_MIN_RESP_SIZE (DHT_COOKIE_SIZE+3)
#define DHT_RENDEZVOUS_REQU_SIZE       (DHT_COOKIE_SIZE+DHT_HASH_SIZE+19)


/** Base class of all search queries.
 * @ingroup internal */
class SearchQuery
{
public:
  /** Constructor. */
  SearchQuery(const Identifier &self, const Identifier &id);
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

protected:
  /** The identifier of the element being searched for. */
  Identifier _id;
  /** The current search queue. */
  QList<NodeItem> _best;
  /** The set of nodes already asked. */
  QSet<Identifier> _queried;
};


/** Base class of all request items. A request item will be stored for every request send. This
 * allows to associate a response (identified by the magic cookie) with a request.
 * @ingroup internal */
class Request
{
public:
  /** Request type. */
  typedef enum {
    PING,              ///< A ping request.
    FIND_NODE,         ///< A find value request.
    FIND_NEIGHBOURS,   ///< A find neighbours request.
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


/** A find value request item.
 * @ingroup internal */
class FindValueRequest: public SearchRequest
{
public:
  /** Constructor.
   * @param query Specifies the query object associated with this request. */
  FindValueRequest(SearchQuery *query);
};


/** A find value request item.
 * @ingroup internal */
class FindNeighboursRequest: public SearchRequest
{
public:
  /** Constructor.
   * @param query Specifies the query object associated with this request. */
  FindNeighboursRequest(SearchQuery *query);
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
   * @param service The service number.
   * @param peer Identifier of the peer node.
   * @param socket The secure socket for the connection. */
  StartConnectionRequest(uint16_t service, const Identifier &peer, SecureSocket *socket);

  /** Returns the socket of the request. */
  inline SecureSocket *socket() const { return _socket; }
  /** Returns the service number of the request. */
  inline uint16_t service() const { return _service; }
  /** Returns the identifier of the remote node. */
  inline const Identifier &peedId() const { return _peer; }

protected:
  /** The service number. */
  uint16_t _service;
  /** The id of the remote node. */
  Identifier _peer;
  /** The socket of the connection. */
  SecureSocket *_socket;
};



/* ******************************************************************************************** *
 * Implementation of SearchQuery etc.
 * ******************************************************************************************** */
SearchQuery::SearchQuery(const Identifier &self, const Identifier &id)
  : _id(id), _best(), _queried()
{
  _queried.insert(self);
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
  while (_best.size() > DHT_K) { _best.pop_back(); }
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

RendezvousSearchRequest::RendezvousSearchRequest(SearchQuery *query)
  : SearchRequest(RENDEZVOUS_SEARCH, query)
{
  // pass...
}

StartConnectionRequest::StartConnectionRequest(uint16_t service, const Identifier &peer, SecureSocket *socket)
  : Request(START_CONNECTION), _service(service), _peer(peer), _socket(socket)
{
  _cookie = socket->id();
}


/* ******************************************************************************************** *
 * Implementation of DHT
 * ******************************************************************************************** */
DHT::DHT(Identity &id,
         const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _self(id), _socket(), _started(false),
    _bytesReceived(0), _lastBytesReceived(0), _inRate(0),
    _bytesSend(0), _lastBytesSend(0), _outRate(0), _buckets(_self.id()),
    _connections(), _requestTimer(), _nodeTimer(), _announcementTimer(), _statisticsTimer()
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

  // check for dead nodes every minute
  _nodeTimer.setInterval(1000*60);
  _nodeTimer.setSingleShot(false);

  // check announcements every 5 minutes
  _announcementTimer.setInterval(1000*60*5);
  _announcementTimer.setSingleShot(false);

  connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  connect(&_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(_onBytesWritten(qint64)));
  connect(&_requestTimer, SIGNAL(timeout()), this, SLOT(_onCheckRequestTimeout()));
  connect(&_nodeTimer, SIGNAL(timeout()), this, SLOT(_onCheckNodeTimeout()));
  connect(&_announcementTimer, SIGNAL(timeout()), this, SLOT(_onCheckAnnouncementTimeout()));
  connect(&_statisticsTimer, SIGNAL(timeout()), this, SLOT(_onUpdateStatistics()));

  _requestTimer.start();
  _nodeTimer.start();
  _announcementTimer.start();
  _statisticsTimer.start();

  _started = true;
}

DHT::~DHT() {
  // pass...
}

void
DHT::ping(const QString &addr, uint16_t port) {
  QHostInfo info = QHostInfo::fromName(addr);
  foreach (QHostAddress addr, info.addresses()) {
    ping(addr, port);
  }
}

void
DHT::ping(const PeerItem &peer) {
  ping(peer.addr(), peer.port());
}

void
DHT::ping(const QHostAddress &addr, uint16_t port) {
  // Create ping request
  PingRequest *req = new PingRequest();
  _pendingRequests.insert(req->cookie(), req);
  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_COOKIE_SIZE);
  memcpy(msg.payload.ping.id, _self.id().data(), DHT_HASH_SIZE);
  msg.payload.ping.type = Message::PING;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, DHT_COOKIE_SIZE+DHT_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send ping to " << addr << ":" << port;
  }
}

void
DHT::ping(const Identifier &id, const QHostAddress &addr, uint16_t port) {
  // Create named ping request
  PingRequest *req = new PingRequest(id);
  _pendingRequests.insert(req->cookie(), req);
  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_COOKIE_SIZE);
  memcpy(msg.payload.ping.id, _self.id().data(), DHT_HASH_SIZE);
  msg.payload.ping.type = Message::PING;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, DHT_COOKIE_SIZE+DHT_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send ping to " << addr << ":" << port;
  }
}

void
DHT::ping(const NodeItem &node) {
  ping(node.id(), node.addr(), node.port());
}

void
DHT::findNode(const Identifier &id) {
  // Create a query instance
  SearchQuery *query = new SearchQuery(_self.id(), id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << id << ". Buckets empty.";
    // Emmit signal if failiour is not a pending announcement
    delete query;
  } else {
    sendFindNode(next, query);
  }
}

void
DHT::rendezvous(const Identifier &id) {
  // Create a query instance
  SearchQuery *query = new SearchQuery(_self.id(), id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << id << ". Buckets empty.";
    // Emmit signal if failiour is not a pending announcement
    emit rendezvousFailed(id);
    delete query;
  } else {
    sendRendezvousSearch(next, query);
  }
}

void
DHT::findNeighbours(const Identifier &id, const QList<NodeItem> &start) {
  // logDebug() << "Search for node " << id;
  // Create a query instance
  SearchQuery *query = new SearchQuery(_self.id(), id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Update with start items
  QList<NodeItem>::const_iterator item = start.begin();
  for (; item != start.end(); item++) { query->update(*item); }
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << id << ". Buckets empty.";
    emit neighboursFound(id, query->best());
    delete query;
  } else {
    sendFindNeighbours(next, query);
  }
}

bool
DHT::hasService(uint16_t service) const {
  return _services.contains(service);
}

bool
DHT::registerService(uint16_t no, AbstractService *handler) {
  if (_services.contains(no)) { return false; }
  _services.insert(no, handler);
  return true;
}

bool
DHT::startConnection(uint16_t service, const NodeItem &node, SecureSocket *stream) {
  logDebug() << "Send start secure connection id=" << stream->id()
             << " to " << node.id()
             << " @" << node.addr() << ":" << node.port();
  StartConnectionRequest *req = new StartConnectionRequest(service, node.id(), stream);

  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_COOKIE_SIZE);
  msg.payload.start_connection.type = Message::START_STREAM;
  msg.payload.start_connection.service = htons(service);
  int keyLen = 0;
  if (0 > (keyLen = stream->prepare(msg.payload.start_connection.pubkey, DHT_MAX_PUBKEY_SIZE)) ) {
    stream->failed();
    delete req;
    return false;
  }

  // Compute total size
  keyLen += DHT_COOKIE_SIZE + 1 + 2;

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
DHT::socketClosed(const Identifier &id) {
  logDebug() << "Secure socket " << id << " closed.";
  _connections.remove(id);
}

Identity &
DHT::identity() {
  return _self;
}

const Identity &
DHT::identity() const {
  return _self;
}

const Identifier &
DHT::id() const {
  return _self.id();
}

bool
DHT::started() const {
  // Check if socket is bound
  return (_started && _socket.isValid() &&
          (QAbstractSocket::BoundState == _socket.state()));
}

size_t
DHT::numNodes() const {
  return _buckets.numNodes();
}

void
DHT::nodes(QList<NodeItem> &lst) {
  _buckets.nodes(lst);
}

size_t
DHT::numSockets() const {
  return _connections.size();
}

size_t
DHT::bytesReceived() const {
  return _bytesReceived;
}

size_t
DHT::bytesSend() const {
  return _bytesSend;
}

double
DHT::inRate() const {
  return _inRate;
}

double
DHT::outRate() const {
  return _outRate;
}


/*
 * Implementation of internal used methods.
 */
void
DHT::sendFindNode(const NodeItem &to, SearchQuery *query) {
  /*logDebug() << "Send FindNode request to " << to.id()
             << " @" << to.addr() << ":" << to.port(); */
  // Construct request item
  FindNodeRequest *req = new FindNodeRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_COOKIE_SIZE);
  msg.payload.find_node.type = Message::FIND_NODE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  int size = DHT_COOKIE_SIZE+1+DHT_K*DHT_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindNode request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
DHT::sendFindNeighbours(const NodeItem &to, SearchQuery *query) {
  /*logDebug() << "Send FindNode request to " << to.id()
             << " @" << to.addr() << ":" << to.port(); */
  // Construct request item
  FindNeighboursRequest *req = new FindNeighboursRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_COOKIE_SIZE);
  msg.payload.find_node.type = Message::FIND_NODE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  int size = DHT_COOKIE_SIZE+1+DHT_K*DHT_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindNode request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
DHT::sendRendezvousSearch(const NodeItem &to, SearchQuery *query) {
  /*logDebug() << "Send FindNode request to " << to.id()
             << " @" << to.addr() << ":" << to.port(); */
  // Construct request item
  RendezvousSearchRequest *req = new RendezvousSearchRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_COOKIE_SIZE);
  msg.payload.find_node.type = Message::FIND_NODE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  int size = DHT_COOKIE_SIZE+1+DHT_K*DHT_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindNode request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
DHT::sendRendezvous(const Identifier &with, const PeerItem &to) {
  Message msg;
  memcpy(msg.cookie, Identifier::create().data(), DHT_COOKIE_SIZE);
  memcpy(msg.payload.rendezvous.id, with.data(), DHT_HASH_SIZE);
  if (DHT_RENDEZVOUS_REQU_SIZE != _socket.writeDatagram(
        (char *)&msg, DHT_RENDEZVOUS_REQU_SIZE, to.addr(), to.port())) {
    logError() << "DHT: Failed to send Rendezvous request to " << to.addr() << ":" << to.port();
  }
}

bool
DHT::sendData(const Identifier &id, const uint8_t *data, size_t len, const PeerItem &peer) {
  return sendData(id, data, len, peer.addr(), peer.port());
}

bool
DHT::sendData(const Identifier &id, const uint8_t *data, size_t len, const QHostAddress &addr, uint16_t port) {
  if (len > DHT_MAX_DATA_SIZE) {
    logError() << "DHT: sendData(): Cannot send connection data: payload too large "
               << len << ">" << DHT_MAX_DATA_SIZE << "!";
    return false;
  }
  // Assemble message
  Message msg;
  memcpy(msg.cookie, id.constData(), DHT_COOKIE_SIZE);
  memcpy(msg.payload.datagram, data, len);
  // send it
  return (qint64(len+DHT_COOKIE_SIZE) ==
          _socket.writeDatagram((const char *)&msg, (len+DHT_COOKIE_SIZE), addr, port));
}

void
DHT::_onReadyRead() {
  while (_socket.hasPendingDatagrams()) {
    // check datagram size
    if ( (_socket.pendingDatagramSize() > DHT_MAX_MESSAGE_SIZE) ||
         (_socket.pendingDatagramSize() < DHT_MIN_MESSAGE_SIZE)) {
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

      Identifier cookie(msg.cookie);
      // First, check if message belongs to a open stream
      if (_connections.contains(cookie)) {
        // Process streams
        _connections[cookie]->handleData(((uint8_t *)&msg)+DHT_COOKIE_SIZE, size-DHT_COOKIE_SIZE);
      } else if (_pendingRequests.contains(cookie)) {
        // Message is a response -> dispatch by type from table
        Request *item = _pendingRequests[cookie];
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
        if ((size == DHT_PING_REQU_SIZE) && (Message::PING == msg.payload.ping.type)){
          _processPingRequest(msg, size, addr, port);
        } else if ((size >= DHT_FIND_NODE_MIN_REQU_SIZE) && (Message::FIND_NODE == msg.payload.find_node.type)) {
          _processFindNodeRequest(msg, size, addr, port);
        } else if ((size > DHT_START_STREAM_MIN_REQU_SIZE) && (Message::START_STREAM == msg.payload.start_connection.type)) {
          _processStartConnectionRequest(msg, size, addr, port);
        } else if ((size == DHT_RENDEZVOUS_REQU_SIZE) && (Message::RENDEZVOUS == msg.payload.rendezvous.type)) {
          _processRendezvousRequest(msg, size, addr, port);
        } else {
          logInfo() << "Unknown request from " << addr << ":" << port
                    << " dropping " << (size-DHT_COOKIE_SIZE) << "b payload.";
        }
      }
    }
  }
}

void
DHT::_onBytesWritten(qint64 n) {
  _bytesSend += n;
}

void
DHT::_onSocketError(QAbstractSocket::SocketState error) {
  logError() << "DHT: Socket state changed: " << _socket.state();
}

void
DHT::_processPingResponse(
    const struct Message &msg, size_t size, PingRequest *req, const QHostAddress &addr, uint16_t port)
{
  // signal success
  emit nodeReachable(NodeItem(msg.payload.ping.id, addr, port));
  // If the buckets are empty -> we are likely bootstrapping
  bool bootstrapping = _buckets.empty();
  // Given that this is a ping response -> add the node to the corresponding
  // bucket if space is left
  if (_buckets.add(msg.payload.ping.id, addr, port)) {
    emit nodeAppeard(NodeItem(msg.payload.ping.id, addr, port));
  }
  if (bootstrapping) {
    emit connected();
    logDebug() << "Still boot strapping: Search for myself.";
    findNode(_self.id());
  }
}

void
DHT::_processFindNodeResponse(
    const struct Message &msg, size_t size, FindNodeRequest *req, const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 == ((size-DHT_FIND_NODE_MIN_RESP_SIZE)%DHT_TRIPLE_SIZE) ) {
    // unpack and update query
    size_t Ntriple = (size-DHT_FIND_NODE_MIN_RESP_SIZE)/DHT_TRIPLE_SIZE;
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
      // delete query
      delete req->query();
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
    delete req->query();
    // done.
    return;
  }

  // Send next request
  sendFindNode(next, req->query());
}

void
DHT::_processFindNeighboursResponse(const Message &msg, size_t size, FindNeighboursRequest *req,
                                    const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 != ((size-DHT_FIND_NODE_MIN_RESP_SIZE)%DHT_TRIPLE_SIZE) ) {
    logInfo() << "Received a malformed FIND_NODE response from "
              << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-DHT_FIND_NEIGHBOR_MIN_RESP_SIZE)/DHT_TRIPLE_SIZE;
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
    delete req->query();
    // done.
    return;
  }
  // Send next request
  sendFindNeighbours(next, req->query());
}

void
DHT::_processRendezvousSearchResponse(const Message &msg, size_t size, RendezvousSearchRequest *req,
                                      const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 != ((size-DHT_FIND_NODE_MIN_RESP_SIZE)%DHT_TRIPLE_SIZE) ) {
    logInfo() << "Received a malformed FIND_NODE response from "
              << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-DHT_FIND_NEIGHBOR_MIN_RESP_SIZE)/DHT_TRIPLE_SIZE;
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
    delete req->query();
    // done.
    return;
  }
  // Send next request
  sendRendezvousSearch(next, req->query());
}

void
DHT::_processStartConnectionResponse(
    const Message &msg, size_t size, StartConnectionRequest *req, const QHostAddress &addr, uint16_t port)
{
  // Verify session key
  if (! req->socket()->verify(msg.payload.start_connection.pubkey, size-DHT_COOKIE_SIZE-3)) {
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

  // Stream started: register stream & notify stream handler
  _connections[req->cookie()] = req->socket();
}

void
DHT::_processPingRequest(
    const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  //logDebug() << "Received Ping request from " << addr << ":" << port;
  // simply assemble a pong response including my own ID
  Message resp;
  memcpy(resp.cookie, msg.cookie, DHT_COOKIE_SIZE);
  memcpy(resp.payload.ping.id, _self.id().data(), DHT_HASH_SIZE);
  resp.payload.ping.type = Message::PING;
  // send
  //logDebug() << "Send Ping response to " << addr << ":" << port;
  if(0 > _socket.writeDatagram((char *) &resp, DHT_PING_RESP_SIZE, addr, port)) {
    logError() << "Failed to send Ping response to " << addr << ":" << port;
  }
  // Add node to candidate nodes for the bucket table if not known already
  if (! _buckets.contains(msg.payload.ping.id)) {
    _buckets.addCandidate(msg.payload.ping.id, addr, port);
  }
}

void
DHT::_processFindNodeRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  QList<NodeItem> best;
  _buckets.getNearest(msg.payload.find_node.id, best);

  struct Message resp;
  // Assemble response
  memcpy(resp.cookie, msg.cookie, DHT_COOKIE_SIZE);
  resp.payload.result.success = 0;
  // Determine the number of nodes to reply
  int N = std::min(std::min(DHT_K, best.size()),
                   int(size-DHT_FIND_NODE_MIN_REQU_SIZE)/DHT_TRIPLE_SIZE);
  //logDebug() << "Assemble FindNode response (N req.: " << N << ")";
  // Add items
  QList<NodeItem>::iterator item = best.begin();
  for (int i = 0; (item!=best.end()) && (i<N); item++, i++) {
    memcpy(resp.payload.result.triples[i].id, item->id().data(), DHT_HASH_SIZE);
    memcpy(resp.payload.result.triples[i].ip, item->addr().toIPv6Address().c, 16);
    resp.payload.result.triples[i].port = htons(item->port());
    /*logDebug() << " add: " << item->id()
               << "@" << item->addr() << ":" << ntohs(resp.payload.result.triples[i].port); */
  }

  // Compute size and send reponse
  size_t resp_size = (DHT_FIND_NODE_MIN_RESP_SIZE + N*DHT_TRIPLE_SIZE);
  _socket.writeDatagram((char *) &resp, resp_size, addr, port);
}

void
DHT::_processStartConnectionRequest(const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  uint16_t service = ntohs(msg.payload.start_connection.service);
  logDebug() << "Received StartConnection request, service: " << service;
  if (! _services.contains(service)) { return; }
  AbstractService *serviceHandler = _services[service];

  // request new connection from service handler
  SecureSocket *connection = 0;
  if (0 == (connection = serviceHandler->newSocket()) ) {
    logInfo() << "Connection handler refuses to create a new connection."; return;
  }

  // verify request
  if (! connection->verify(msg.payload.start_connection.pubkey, size-DHT_COOKIE_SIZE-3)) {
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
  memcpy(resp.cookie, msg.cookie, DHT_COOKIE_SIZE);
  resp.payload.start_connection.type = Message::START_STREAM;
  resp.payload.start_connection.service = msg.payload.start_connection.service;
  if (0 > (keyLen = connection->prepare(resp.payload.start_connection.pubkey, DHT_MAX_PUBKEY_SIZE))) {
    logError() << "Can not prepare connection.";
    delete connection; return;
  }

  if (! connection->start(resp.cookie, PeerItem(addr, port))) {
    logError() << "Can not finish SecureSocket handshake.";
    delete connection; return;
  }

  // compute message size
  keyLen += DHT_START_STREAM_MIN_RESP_SIZE;
  // Send response
  if (keyLen != _socket.writeDatagram((char *)&resp, keyLen, addr, port)) {
    logError() << "Can not send StartConnection response";
    delete connection; return;
  }

  // Connection started..
  _connections[resp.cookie] = connection;
  serviceHandler->connectionStarted(connection);
}

void
DHT::_processRendezvousRequest(Message &msg, size_t size, const QHostAddress &addr, uint16_t port) {
  if (_self.id() == Identifier(msg.payload.rendezvous.id)) {
    // If the rendezvous request addressed me -> response with a ping
    ping(msg.payload.rendezvous.ip, ntohs(msg.payload.rendezvous.port));
  } else if (_buckets.contains(msg.payload.rendezvous.id)) {
    // If the rendezvous request is not addressed to me but to a node I know -> forward
    NodeItem node = _buckets.getNode(msg.payload.rendezvous.id);
    memcpy(msg.payload.rendezvous.ip, addr.toIPv6Address().c, 16);
    msg.payload.rendezvous.port = htons(port);
    if (DHT_RENDEZVOUS_REQU_SIZE != _socket.writeDatagram(
          (char *)&msg, DHT_RENDEZVOUS_REQU_SIZE, node.addr(), node.port())) {
      logError() << "DHT: Cannot forward rendezvous request to " << node.id()
                 << " @" << node.addr() << ":" << node.port();
    }  
  }
  // silently ignore rendezvous requests to an unknown node.
}

void
DHT::_onCheckRequestTimeout() {
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
      if (! ping->id().isNull()) {
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
        // delete request and query
        delete *req; delete query;
      } else {
        // Continue search
        sendFindNode(next, query); delete *req;
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
        // delete request & query
        delete *req; delete query;
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
        } else {
          // if not -> failed.
          emit rendezvousFailed(query->id());
        }
        // delete request & query
        delete *req; delete query;
      } else {
        // Send next request
        sendRendezvousSearch(next, query);
        delete *req;
      }
    } else if (Request::START_CONNECTION == (*req)->type()) {
      logDebug() << "StartConnection request timeout...";
      static_cast<StartConnectionRequest *>(*req)->socket()->failed();
      delete *req;
    }
  }
}

void
DHT::_onCheckNodeTimeout() {
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
DHT::_onUpdateStatistics() {
  _inRate = (double(_bytesReceived - _lastBytesReceived)/_statisticsTimer.interval())/1000;
  _lastBytesReceived = _bytesReceived;
  _outRate = (double(_bytesSend - _lastBytesSend)/_statisticsTimer.interval())/1000;
  _lastBytesSend = _bytesSend;
}
