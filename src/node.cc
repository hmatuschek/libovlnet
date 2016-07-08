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
    SEARCH,
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
      uint8_t type;                   ///< Type flag == @c MSG_PING.
      char    id[OVL_HASH_SIZE];      ///< Identifier of the sender.
      char    network[OVL_HASH_SIZE]; ///< Identifier of the network.
    } ping;

    /** A find node request. */
    struct __attribute__((packed)) {
      /** Type flag == @c SEARCH. */
      uint8_t type;
      /** The identifier of the node to find. */
      char    id[OVL_HASH_SIZE];
      /** The identifier of the network to search. */
      char    network[OVL_HASH_SIZE];
      /** This dummy payload is needed to avoid the risk to exploid this request for a relay DoS
       * attack. It ensures that the request has at least the same size as the response. The size
       * of this field implicitly defines the maximal number of triples returned by the remote node. */
      char    dummy[OVL_MAX_TRIPLES*OVL_TRIPLE_SIZE-2*OVL_HASH_SIZE-1];
    } search;

    /** A response to "search". */
    struct __attribute__((packed)) {
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
    SEARCH,            ///< A find node request.
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
  PingRequest(const Identifier &netid);
  /** Named ping request. */
  PingRequest(const Identifier &id, const Identifier &netid);

  /** Returns the identifier of the node if this is a named ping request. */
  inline const Identifier &id() const { return _id; }
  /** Returns the network id for the ping request. */
  inline const Identifier &netid() const { return _prefix; }

protected:
  /** Identifier of the node if this is a named ping request. */
  Identifier _id;
  /** Identifier of the prefix/subnet for the ping. */
  Identifier _prefix;
};


/** Represents all search requests.
 * @ingroup internal */
class SearchRequest: public Request
{
public:
  /** Hidden constructor. */
  SearchRequest(SearchQuery *query);
  /** Returns the query instance associated with the request. */
  inline SearchQuery *query() const { return _query; }

protected:
  /** The search query associated with the request. */
  SearchQuery *_query;
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
 * Implementation of ValueSearchQuery
 * ******************************************************************************************** */
RendezvousSearchQuery::RendezvousSearchQuery(Node &node, const Identifier &id)
  : NeighbourhoodQuery(id), _node(node)
{
  // pass...
}

bool
RendezvousSearchQuery::next(NodeItem &node) {
  if (NeighbourhoodQuery::next(node)) {
    _node.sendRendezvous(_id, node);
    return true;
  }
  return false;
}

/* ******************************************************************************************** *
 * Implementation of Request etc.
 * ******************************************************************************************** */
Request::Request(Type type)
  : _type(type), _cookie(Identifier::create()), _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

PingRequest::PingRequest(const Identifier &netid)
  : Request(PING), _id(), _prefix(netid)
{
  // pass...
}

PingRequest::PingRequest(const Identifier &id, const Identifier &netid)
  : Request(PING), _id(id), _prefix(netid)
{
  // pass...
}

SearchRequest::SearchRequest(SearchQuery *query)
  : Request(SEARCH), _query(query)
{
  // pass...
}

StartConnectionRequest::StartConnectionRequest(const Identifier &service, const Identifier &peer, SecureSocket *socket)
  : Request(START_CONNECTION), _service(service), _peer(peer), _socket(socket)
{
  _cookie = socket->id();
}


/* ******************************************************************************************** *
 * Implementation of Node
 * ******************************************************************************************** */
Node::Node(const Identity &id,
           const QHostAddress &addr, quint16 port, QObject *parent)
  : Network(id.id(), parent), _self(id), _socket(), _started(false),
    _prefix(""), _networks(),
    _bytesReceived(0), _lastBytesReceived(0), _inRate(0),
    _bytesSend(0), _lastBytesSend(0), _outRate(0),
    _connections(), _requestTimer(), _rendezvousTimer(), _statisticsTimer()
{
  qsrand(QDateTime::currentDateTime().currentMSecsSinceEpoch());
  logInfo() << "Start node #" << id.id() << " @ " << addr << ":" << port;

  // register myself as a network
  _networks.insert(this->netid(), this);

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

  // Check for dead announcements and check for update my announcement items every 3min
  connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  connect(&_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(_onBytesWritten(qint64)));
  connect(&_requestTimer, SIGNAL(timeout()), this, SLOT(_onCheckRequestTimeout()));
  connect(&_rendezvousTimer, SIGNAL(timeout()), this, SLOT(_onPingRendezvousNodes()));
  connect(&_statisticsTimer, SIGNAL(timeout()), this, SLOT(_onUpdateStatistics()));

  _requestTimer.start();
  _rendezvousTimer.start();
  _statisticsTimer.start();

  _started = true;
}

Node::~Node() {
  // pass...
}

bool
Node::hasNetwork(const QString &prefix) const {
  char hash[OVL_HASH_SIZE];
  QByteArray prefName = prefix.toUtf8();
  OVLHash((const uint8_t *)prefName.constData(), prefName.size(), (uint8_t *)hash);
  return _networks.contains(Identifier(hash));
}

bool
Node::registerNetwork(Network *subnet) {
  if (_networks.contains(subnet->netid()))
    return false;
  subnet->setParent(this);
  _networks.insert(subnet->netid(), subnet);
  return true;
}

Network *
Node::network(const QString &prefix) {
  char hash[OVL_HASH_SIZE];
  QByteArray prefName = prefix.toUtf8();
  OVLHash((const uint8_t *)prefName.constData(), prefName.size(), (uint8_t *)hash);
  return _networks[Identifier(hash)];
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
  sendPing(addr, port, netid());
}

void
Node::ping(const Identifier &id, const QHostAddress &addr, uint16_t port) {
  sendPing(id, addr, port, netid());
}

void
Node::ping(const NodeItem &node) {
  sendPing(node.id(), node.addr(), node.port(), netid());
}

void
Node::search(SearchQuery *query) {
  query->ignore(_self.id());
  // Collect DHT_K nearest nodes
  _buckets.getNearest(query->id(), query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not search for " << query->id() << ". Buckets empty.";
    query->searchFailed();
  } else {
    sendSearch(next, query);
  }
}

void
Node::rendezvous(const Identifier &id) {
  // Create a query instance
  SearchQuery *query = new RendezvousSearchQuery(*this, id);
  query->ignore(_self.id());
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << id << ". Buckets empty.";
    query->searchCompleted();
  } else {
    sendSearch(next, query);
  }
}

bool
Node::hasService(const QString &service) const {
  unsigned char hash[OVL_HASH_SIZE];
  QByteArray service_name = service.toUtf8();
  OVLHash((const unsigned char *)service_name.data(), service_name.size(), hash);
  return _services.contains(Identifier((char *)hash));
}

bool
Node::registerService(const QString &service, AbstractService *handler) {
  unsigned char hash[OVL_HASH_SIZE];
  QByteArray sName = service.toUtf8();
  OVLHash((const uint8_t *)sName.constData(), sName.size(), hash);
  Identifier id((const char *) hash);
  if (_services.contains(id))
    return false;
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

Node &
Node::root() {
  return *this;
}

const QString &
Node::prefix() const {
  return _prefix;
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
Node::sendPing(const QHostAddress &addr, uint16_t port, const Identifier &netid) {
  // Create named ping request
  PingRequest *req = new PingRequest(netid);
  _pendingRequests.insert(req->cookie(), req);
  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  memcpy(msg.payload.ping.id, _self.id().constData(), OVL_HASH_SIZE);
  memcpy(msg.payload.ping.network, netid.constData(), OVL_HASH_SIZE);
  msg.payload.ping.type = Message::PING;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, OVL_COOKIE_SIZE+OVL_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send ping to " << addr << ":" << port;
  }
}

void
Node::sendPing(const Identifier &id, const QHostAddress &addr, uint16_t port, const Identifier &netid) {
  // Create named ping request
  PingRequest *req = new PingRequest(id, netid);
  _pendingRequests.insert(req->cookie(), req);
  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  memcpy(msg.payload.ping.id, _self.id().constData(), OVL_HASH_SIZE);
  memcpy(msg.payload.ping.network, netid.constData(), OVL_HASH_SIZE);
  msg.payload.ping.type = Message::PING;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, OVL_COOKIE_SIZE+OVL_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send ping to " << addr << ":" << port;
  }
}

void
Node::sendSearch(const NodeItem &to, SearchQuery *query) {
  // Construct request item
  SearchRequest *req = new SearchRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), OVL_COOKIE_SIZE);
  msg.payload.search.type = Message::SEARCH;
  memcpy(msg.payload.search.id, query->id().constData(), OVL_HASH_SIZE);
  memcpy(msg.payload.search.network, query->netid().constData(), OVL_HASH_SIZE);
  int size = OVL_COOKIE_SIZE+1+OVL_HASH_SIZE+OVL_K*OVL_TRIPLE_SIZE;
  if (size != _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send Search request to " << to.id()
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
      // Update RX statistics
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
        } else if (Request::SEARCH == item->type()) {
          _processSearchResponse(msg, size, static_cast<SearchRequest *>(item), addr, port);
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
        } else if ((size >= OVL_SEARCH_MIN_REQU_SIZE) && (Message::SEARCH == msg.payload.search.type)) {
          _processSearchRequest(msg, size, addr, port);
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
Node::_processPingResponse(const struct Message &msg, size_t size, PingRequest *req,
                           const QHostAddress &addr, uint16_t port)
{
  // check if network identifier of response matches request network
  Identifier remoteNetId(msg.payload.ping.network);
  if (remoteNetId != req->netid())
    return;
  // Irrespective of the network, handle node reachable event
  this->nodeReachableEvent(NodeItem(Identifier(msg.payload.ping.id), addr, port));
  // Then, check if network is known
  if (! _networks.contains(remoteNetId))
    return;
  // if so -> notify network
  _networks[remoteNetId]->nodeReachableEvent(NodeItem(Identifier(msg.payload.ping.id), addr, port));
}

void
Node::_processSearchResponse(
    const struct Message &msg, size_t size, SearchRequest *req, const QHostAddress &addr, uint16_t port)
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
      // add candidate for the specific network
      if (_networks.contains(req->query()->netid()))
        _networks[req->query()->netid()]->addCandidate(item);
      // Update node list of query
      req->query()->update(item);
    }

    // If the search has been completed -> done
    if (req->query()->isSearchComplete()) {
      req->query()->searchCompleted();
      return;
    }
  } else {
    logInfo() << "Received a malformed Search response from "
              << addr << ":" << port;
  }
  // Get next node to query
  NodeItem next;
  // get next node to query, if there is no next node -> search failed
  if (! req->query()->next(next)) {
    // delete query
    req->query()->searchCompleted();
    // done.
    return;
  }

  // Send next request
  sendSearch(next, req->query());
}

void
Node::_processStartConnectionResponse(
    const Message &msg, size_t size, StartConnectionRequest *req, const QHostAddress &addr, uint16_t port)
{
  // Verify session key
  if (! req->socket()->verify(msg.payload.start_connection.pubkey, size-OVL_COOKIE_SIZE-3)) {
    logError() << "Verification of peer session key failed for connection id="
               << req->socket()->id().toBase32() << ".";
    req->socket()->failed();
    return;
  }

  // verify fingerprints
  if (!(req->socket()->peerId() == req->peedId())) {
    logError() << "Peer fingerprint mismatch: " << req->socket()->peerId()
               << " != " << req->peedId() << " for connection id="
               << req->socket()->id().toBase32() << ".";
    req->socket()->failed();
    return;
  }

  // success -> start connection
  if (! req->socket()->start(req->cookie(), PeerItem(addr, port))) {
    logError() << "Can not initialize symmetric chipher for connection id="
               << req->socket()->id().toBase32() << ".";
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
  Identifier remoteNetId(msg.payload.ping.network);
  if (! _networks.contains(remoteNetId)) {
    logDebug() << "Received Ping request from " << Identifier(msg.payload.ping.id)
               << "@" << addr << ":" << port
               << " for unknown network " << remoteNetId << ".";
    // Anyway, add as a candidate to the root network
    _buckets.addCandidate(Identifier(msg.payload.ping.id), addr, port);
    return;
  }

  // simply assemble a pong response including my own ID
  Message resp;
  memcpy(resp.cookie, msg.cookie, OVL_COOKIE_SIZE);
  memcpy(resp.payload.ping.id, _self.id().constData(), OVL_HASH_SIZE);
  memcpy(resp.payload.ping.network, remoteNetId.constData(), OVL_HASH_SIZE);
  resp.payload.ping.type = Message::PING;
  // send
  //logDebug() << "Send Ping response to " << addr << ":" << port;
  if(0 > _socket.writeDatagram((char *) &resp, OVL_PING_RESP_SIZE, addr, port)) {
    logError() << "Failed to send Ping response to " << addr << ":" << port;
  }

  // Add node to candidate nodes for the specific network
  _networks[remoteNetId]->addCandidate(NodeItem(Identifier(msg.payload.ping.id), addr, port));

}

void
Node::_processSearchRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  QList<NodeItem> best;
  _buckets.getNearest(Identifier(msg.payload.search.id), best);

  struct Message resp;
  // Assemble response
  memcpy(resp.cookie, msg.cookie, OVL_COOKIE_SIZE);
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
    logDebug() << "Received rendezvous request -> ping back.";
    // If the rendezvous request addressed me -> response with a ping
    ping(msg.payload.rendezvous.ip, ntohs(msg.payload.rendezvous.port));
  } else if (_buckets.contains(Identifier(msg.payload.rendezvous.id))) {
    // If the rendezvous request is not addressed to me but to a node I know -> forward
    NodeItem node = _buckets.getNode(Identifier(msg.payload.rendezvous.id));
    memcpy(msg.payload.rendezvous.ip, addr.toIPv6Address().c, 16);
    msg.payload.rendezvous.port = htons(port);
    logDebug() << "Forward rendezvous to " << node.id() << ".";
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
    } else if (Request::SEARCH == (*req)->type()) {
      logDebug() << "Search request timeout...";
      SearchQuery *query = static_cast<SearchRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> search failed
      if (! query->next(next)) {
        query->searchCompleted();
        // delete request and query
        delete *req;
      } else {
        // Continue search
        sendSearch(next, query); delete *req;
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
Node::_onPingRendezvousNodes() {
  QList<NodeItem> nodes;
  _buckets.getNearest(id(), nodes);
  QList<NodeItem>::iterator node = nodes.begin();
  for (; node != nodes.end(); node++) {
    ping(*node);
  }
}

void
Node::_onUpdateStatistics() {
  _inRate = (double(_bytesReceived - _lastBytesReceived)/_statisticsTimer.interval())*1000;
  _lastBytesReceived = _bytesReceived;
  _outRate = (double(_bytesSend - _lastBytesSend)/_statisticsTimer.interval())*1000;
  _lastBytesSend = _bytesSend;
}
