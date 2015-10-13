#include "dht.h"
#include "crypto.h"
#include "dht_config.h"

#include <QHostInfo>
#include <netinet/in.h>
#include <inttypes.h>


/** Possible message types. */
typedef enum {
  MSG_PING = 0,
  MSG_ANNOUNCE,
  MSG_FIND_NODE,
  MSG_FIND_VALUE,
  MSG_START_STREAM,
} MessageType;

/** Represents a triple of ID, IP address and port as transferred via UDP. */
struct __attribute__((packed)) DHTTriple {
  /** The ID of a node. */
  char      id[DHT_HASH_SIZE];
  /** The IP of the node. */
  uint8_t   ip[16];
  /** The port of the node. */
  uint16_t  port;
};

/** The structure of the UDP datagrams transferred. */
struct __attribute__((packed)) Message
{
  /** The magic cookie to match a response to a request. */
  char    cookie[DHT_HASH_SIZE];

  /** Payload */
  union __attribute__((packed)) {
    struct __attribute__((packed)){
      uint8_t type; // == PING;
      char    id[DHT_HASH_SIZE];
    } ping;

    struct __attribute__((packed)) {
      uint8_t type; // == ANNOUNCE
      char    who[DHT_HASH_SIZE];
      char    what[DHT_HASH_SIZE];
    } announce;

    struct __attribute__((packed)) {
      uint8_t type; // == FIND_NODE
      /** The identifier of the node to find. */
      char    id[DHT_HASH_SIZE];
      /** This dummy payload is needed to avoid the risk to exploid this request for a relay DoS
       * attack. It ensures that the request has at least the same size as the response. The size
       * of this field implicitly defines the number of triples returned by the remote node. */
      char    dummy[DHT_MAX_TRIPLES*DHT_TRIPLE_SIZE-DHT_HASH_SIZE];
    } find_node;

    struct __attribute__((packed)) {
      uint8_t type; // == FIND_VALUE
      /** The identifier of the value to find. */
      char    id[DHT_HASH_SIZE];
      /** This dummy payload is needed to avoid the risk to exploid this request for a relay DoS
       * attack. It ensures that the request has at least the same size as the response. The size
       * of this field implicitly defines the number of triples returned by the remote node. */
      char    dummy[DHT_MAX_TRIPLES*DHT_TRIPLE_SIZE-DHT_HASH_SIZE];
    } find_value;

    struct __attribute__((packed)) {
      uint8_t   success;
      DHTTriple triples[DHT_MAX_TRIPLES];
    } result;

    struct __attribute__((packed)) {
      uint8_t  type;      // == START_STREAM
      /** A stream service id (not part of the DHT specification). */
      uint16_t service;
      /** Public (ECDH) key of the requesting or responding node. */
      uint8_t  pubkey[DHT_MAX_PUBKEY_SIZE];
    } start_stream;

    /** A stream datagram. */
    uint8_t datagram[DHT_MAX_DATA_SIZE];
  } payload;

  Message();
};

Message::Message()
{
  memset(this, 0, sizeof(Message));
}


class SearchQuery
{
protected:
  SearchQuery(const Identifier &id);

public:
  const Identifier &id() const;
  void update(const NodeItem &nodes);
  bool next(NodeItem &node);
  QList<NodeItem> &best();
  const QList<NodeItem> &best() const;
  const NodeItem &first() const;

protected:
  Identifier _id;
  QList<NodeItem> _best;
  QSet<Identifier> _queried;
};


class FindNodeQuery: public SearchQuery
{
public:
  FindNodeQuery(const Identifier &id);

  bool found() const;
};


class FindValueQuery: public SearchQuery
{
public:
  FindValueQuery(const Identifier &id);
};


class Request
{
protected:
  Request(MessageType type);

public:
  inline MessageType type() const { return _type; }
  inline const Identifier &cookie() const { return _cookie; }
  inline const QDateTime &timestamp() const { return _timestamp; }
  inline size_t olderThan(size_t seconds) const {
    return (_timestamp.addMSecs(seconds) < QDateTime::currentDateTime());
  }

protected:
  MessageType _type;
  Identifier    _cookie;
  QDateTime     _timestamp;
};


class PingRequest: public Request
{
public:
  PingRequest();
};

class FindNodeRequest: public Request
{
public:
  FindNodeRequest(FindNodeQuery *query);
  inline FindNodeQuery *query() const { return _findNodeQuery; }

protected:
  FindNodeQuery *_findNodeQuery;
};


class FindValueRequest: public Request
{
public:
  FindValueRequest(FindValueQuery *query);
  inline FindValueQuery *query() const { return _findValueQuery; }

protected:
  FindValueQuery *_findValueQuery;
};

class StartStreamRequest: public Request
{
public:
  StartStreamRequest(uint16_t service, const Identifier &peer, SecureSocket *stream);

  inline SecureSocket *query() const { return _stream; }
  inline uint16_t service() const { return _service; }
  inline const Identifier &peedId() const { return _peer; }

protected:
  uint16_t _service;
  Identifier _peer;
  SecureSocket *_stream;
};



/* ******************************************************************************************** *
 * Implementation of SearchQuery etc.
 * ******************************************************************************************** */
SearchQuery::SearchQuery(const Identifier &id)
  : _id(id), _best()
{
  // pass...
}

const Identifier &
SearchQuery::id() const {
  return _id;
}

void
SearchQuery::update(const NodeItem &node) {
  logDebug() << "Update search list with " << node.id()
             << " @ " << node.addr() << ":" << node.port();
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


FindNodeQuery::FindNodeQuery(const Identifier &id)
  : SearchQuery(id)
{
  // pass...
}

bool
FindNodeQuery::found() const {
  if (0 == _best.size()) { return false; }
  return _id == _best.front().id();
}

FindValueQuery::FindValueQuery(const Identifier &id)
  : SearchQuery(id)
{
  // pass...
}


/* ******************************************************************************************** *
 * Implementation of Request etc.
 * ******************************************************************************************** */
Request::Request(MessageType type)
  : _type(type), _cookie(), _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

PingRequest::PingRequest()
  : Request(MSG_PING)
{
  // pass...
}

FindNodeRequest::FindNodeRequest(FindNodeQuery *query)
  : Request(MSG_FIND_NODE), _findNodeQuery(query)
{
  // pass...
}

FindValueRequest::FindValueRequest(FindValueQuery *query)
  : Request(MSG_FIND_VALUE), _findValueQuery(query)
{
  // pass...
}

StartStreamRequest::StartStreamRequest(uint16_t service, const Identifier &peer, SecureSocket *stream)
  : Request(MSG_START_STREAM), _service(service), _peer(peer), _stream(stream)
{
  // pass...
}


/* ******************************************************************************************** *
 * Implementation of DHT
 * ******************************************************************************************** */
DHT::DHT(Identity &id, SocketHandler *streamHandler,
         const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _self(id), _socket(), _bytesReceived(0), _lastBytesReceived(0), _inRate(0),
    _bytesSend(0), _lastBytesSend(0), _outRate(0), _buckets(_self.id()),
    _streamHandler(streamHandler), _streams(),
    _requestTimer(), _nodeTimer(), _announcementTimer(), _statisticsTimer()
{
  logInfo() << "Start node #" << id.id() << " @ " << addr << ":" << port;

  if (!_socket.bind(addr, port)) {
    logError() << "Cannot bind to " << addr << ":" << port;
    return;
  }

  // check request timeouts every 500ms
  _requestTimer.setInterval(500);
  _requestTimer.setSingleShot(false);
  // check for dead nodes every minute
  _nodeTimer.setInterval(1000*60);
  _nodeTimer.setSingleShot(false);
  // check announcements every 5 minutes
  _announcementTimer.setInterval(1000*60*5);
  _announcementTimer.setSingleShot(false);
  // Update statistics every 5 seconds
  _statisticsTimer.setInterval(1000*5);
  _statisticsTimer.setSingleShot(false);

  QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  QObject::connect(&_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(_onBytesWritten(qint64)));
  QObject::connect(&_requestTimer, SIGNAL(timeout()), this, SLOT(_onCheckRequestTimeout()));
  QObject::connect(&_nodeTimer, SIGNAL(timeout()), this, SLOT(_onCheckNodeTimeout()));
  QObject::connect(&_announcementTimer, SIGNAL(timeout()), this, SLOT(_onCheckAnnouncementTimeout()));
  QObject::connect(&_statisticsTimer, SIGNAL(timeout()), this, SLOT(_onUpdateStatistics()));

  _requestTimer.start();
  _nodeTimer.start();
  _announcementTimer.start();
  _statisticsTimer.start();
}

DHT::~DHT() {
  // pass...
}

void
DHT::ping(const QString &addr, uint16_t port) {
  QHostInfo info = QHostInfo::fromName(addr);
  if (info.addresses().size()) {
    ping(info.addresses().front(), port);
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
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  memcpy(msg.payload.ping.id, _self.id().data(), DHT_HASH_SIZE);
  msg.payload.ping.type = MSG_PING;
  logDebug() << "Send ping to " << addr << ":" << port;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, 2*DHT_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send ping to " << addr << ":" << port;
  }
}

void
DHT::findNode(const Identifier &id) {
  logDebug() << "Search for node " << id;
  // Create a query instance
  FindNodeQuery *query = new FindNodeQuery(id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find node" << id << ". Buckets empty.";
    // Emmit signal if failiour is not a pending announcement
    if (! isPendingAnnouncement(id)) {
      emit nodeNotFound(id, query->best());
    }
    delete query;
  } else {
    sendFindNode(next, query);
  }
}

void
DHT::findValue(const Identifier &id) {
  logDebug() << "Search for value " << id;
  // Create a query instance
  FindValueQuery *query = new FindValueQuery(id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    logInfo() << "Can not find value " << id << ". Buckets empty.";
    delete query; emit valueNotFound(id);
  } else {
    sendFindValue(next, query);
  }
}

void
DHT::announce(const Identifier &id) {
  logDebug() << "Announce data " << id << " to the world.";
  _announcedData[id] = QDateTime();
  // lets search for the nodes closest to the data id
  findNode(id);
}

bool
DHT::startStream(uint16_t service, const NodeItem &node, SecureSocket *stream) {
  if (0 == _streamHandler) { delete stream; return false; }
  logDebug() << "Send start secure connection to " << node.id()
             << " @" << node.addr() << ":" << node.port();
  if (! stream) { delete stream; return false; }
  StartStreamRequest *req = new StartStreamRequest(service, node.id(), stream);

  // Assemble message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  msg.payload.start_stream.type = MSG_START_STREAM;
  msg.payload.start_stream.service = htons(service);
  int keyLen = 0;
  if (0 > (keyLen = stream->prepare(msg.payload.start_stream.pubkey, DHT_MAX_PUBKEY_SIZE)) ) {
    delete stream; delete req; return false;
  }

  // Compute total size
  keyLen += DHT_HASH_SIZE + 1 + 2;

  // add to pending request list & send it
  _pendingRequests.insert(req->cookie(), req);
  if (keyLen == _socket.writeDatagram((char *)&msg, keyLen, node.addr(), node.port())) {
    return true;
  }
  _pendingRequests.remove(req->cookie());
  delete stream; delete req; return false;
}

void
DHT::socketClosed(const Identifier &id) {
  logDebug() << "Secure socket " << id << " closed.";
  _streams.remove(id);
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

size_t
DHT::numNodes() const {
  return _buckets.numNodes();
}

void
DHT::nodes(QList<NodeItem> &lst) {
  _buckets.nodes(lst);
}

size_t
DHT::numKeys() const {
  return _announcements.size();
}

size_t
DHT::numData() const {
  return _announcedData.size();
}

size_t
DHT::numStreams() const {
  return _streams.size();
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
DHT::sendFindNode(const NodeItem &to, FindNodeQuery *query) {
  logDebug() << "Send FindNode request to " << to.id()
             << " @" << to.addr() << ":" << to.port();
  // Construct request item
  FindNodeRequest *req = new FindNodeRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  msg.payload.find_node.type = MSG_FIND_NODE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  size_t size = DHT_HASH_SIZE+1+DHT_K*DHT_TRIPLE_SIZE;
  if(0 > _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindNode request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

void
DHT::sendFindValue(const NodeItem &to, FindValueQuery *query) {
  logDebug() << "Send FindValue request to " << to.id()
             << " @" << to.addr() << ":" << to.port();
  // Construct request item
  FindValueRequest *req = new FindValueRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  msg.payload.find_node.type = MSG_FIND_VALUE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  size_t size = DHT_HASH_SIZE+1+DHT_K*DHT_TRIPLE_SIZE;
  if (0 > _socket.writeDatagram((char *)&msg, size, to.addr(), to.port())) {
    logError() << "Failed to send FindNode request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
}

bool
DHT::isPendingAnnouncement(const Identifier &id) const {
  return _announcedData.contains(id);
}

void
DHT::sendAnnouncement(const NodeItem &to, const Identifier &what) {
  logDebug() << "Send Announcement of " << what << " to " << to.id()
             << " @" << to.addr() << ":" << to.port();
  // Assemble & send message
  struct Message msg;
  memcpy(msg.cookie, Identifier().data(), DHT_HASH_SIZE);
  memcpy(msg.payload.announce.what, what.data(), DHT_HASH_SIZE);
  memcpy(msg.payload.announce.who, _self.id().data(), DHT_HASH_SIZE);
  msg.payload.announce.type = MSG_ANNOUNCE;
  if (0 > _socket.writeDatagram((char *)&msg, 3*DHT_HASH_SIZE+1, to.addr(), to.port())) {
    logError() << "Failed to send Announce request to " << to.id()
               << " @" << to.addr() << ":" << to.port();
  }
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
      if (_streams.contains(cookie)) {
        // Process streams
        _streams[cookie]->handleData(((uint8_t *)&msg)+DHT_HASH_SIZE, size-DHT_HASH_SIZE);
      } else if (_pendingRequests.contains(cookie)) {
        // Message is a response -> dispatch by type from table
        Request *item = _pendingRequests[cookie];
        _pendingRequests.remove(cookie);
        if (MSG_PING == item->type()) {
          _processPingResponse(msg, size, static_cast<PingRequest *>(item), addr, port);
        } else if (MSG_FIND_NODE == item->type()) {
          _processFindNodeResponse(msg, size, static_cast<FindNodeRequest *>(item), addr, port);
        } else if (MSG_FIND_VALUE == item->type()) {
          _processFindValueResponse(msg, size, static_cast<FindValueRequest *>(item), addr, port);
        } else if (MSG_START_STREAM == item->type()) {
          _processStartStreamResponse(msg, size, static_cast<StartStreamRequest *>(item), addr, port);
        }else {
          logInfo() << "Unknown response from " << addr << ":" << port;
        }
        delete item;
      } else {
        // Message is likely a request
        if ((size == (2*DHT_HASH_SIZE+1)) && (MSG_PING == msg.payload.ping.type)){
          _processPingRequest(msg, size, addr, port);
        } else if ((size >= (DHT_HASH_SIZE+1)) && (MSG_FIND_NODE == msg.payload.find_node.type)) {
          _processFindNodeRequest(msg, size, addr, port);
        } else if ((size >= (DHT_HASH_SIZE+1)) && (MSG_FIND_VALUE == msg.payload.find_value.type)) {
          _processFindValueRequest(msg, size, addr, port);
        } else if ((size == (3*DHT_HASH_SIZE+1)) && (MSG_ANNOUNCE == msg.payload.announce.type)) {
          _processAnnounceRequest(msg, size, addr, port);
        } else if ((size > (DHT_HASH_SIZE+3)) && (MSG_START_STREAM == msg.payload.announce.type)) {
          _processStartStreamRequest(msg, size, addr, port);
        } else {
          logInfo() << "Unknown request from " << addr << ":" << port
                    << " dropping " << (size-DHT_HASH_SIZE) << "b payload.";
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
DHT::_processPingResponse(
    const struct Message &msg, size_t size, PingRequest *req, const QHostAddress &addr, uint16_t port)
{
  logDebug() << "Received Ping response from " << addr << ":" << port;
  // sinal success
  emit nodeReachable(NodeItem(msg.payload.ping.id, addr, port));
  // If the buckets are empty -> we are likely bootstrapping
  bool bootstrapping = _buckets.empty();
  // Given that this is a ping response -> add the node to the corresponding
  // bucket if space is left
  _buckets.add(msg.payload.ping.id, addr, port);
  if (bootstrapping) {
    logDebug() << "Still boot strapping: Search for myself.";
    findNode(_self.id());
  }
}

void
DHT::_processFindNodeResponse(
    const struct Message &msg, size_t size, FindNodeRequest *req, const QHostAddress &addr, uint16_t port)
{
  logDebug() << "Received FindNode response from " << addr << ":" << port;
  // payload length must be a multiple of triple length
  if ( 0 == ((size-DHT_HASH_SIZE-1)%DHT_TRIPLE_SIZE) ) {
    // unpack and update query
    size_t Ntriple = (size-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE;
    logDebug() << "Received " << Ntriple << " nodes from "  << addr << ":" << port;
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      NodeItem item(id, QHostAddress((const Q_IPV6ADDR &)*(msg.payload.result.triples[i].ip)),
                    ntohs(msg.payload.result.triples[i].port));
      logDebug() << " got: " << item.id() << "@" << item.addr() << ":" << ntohs(msg.payload.result.triples[i].port);
      // Add discovered node to buckets
      _buckets.add(id, item.addr(), item.port());
      // Update node list of query
      req->query()->update(item);
    }

    // If the node was found -> signal success
    if (req->query()->found()) {
      logDebug() << "Found node " << req->query()->first().id()
                 << " @" << req->query()->first().addr() << ":" << req->query()->first().port();
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
    if (isPendingAnnouncement(req->query()->id())) {
      // -> send announcement to the best nodes
      QList<NodeItem>::iterator node = req->query()->best().begin();
      for (; node != req->query()->best().end(); node++) {
        sendAnnouncement(*node, req->query()->id());
      }
    } else {
      logInfo() << "Node " << req->query()->id() << " not found.";
      // if it was a regular node search -> signal error
      emit nodeNotFound(req->query()->id(), req->query()->best());
    }
    // delete query
    delete req->query();
    // done.
    return;
  }

  logDebug() << "Node " << req->query()->id() << " not found yet -> continue with "
             << next.id() << " @" << next.addr() << ":" << next.port();
  // Send next request
  sendFindNode(next, req->query());
}

void
DHT::_processFindValueResponse(
    const struct Message &msg, size_t size, FindValueRequest *req, const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 != ((size-DHT_HASH_SIZE-1)%DHT_TRIPLE_SIZE) ) {
    logInfo() << "Received a malformed FIND_NODE response from "
              << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE;

    // If queried node has value
    if (msg.payload.result.success) {
      // Get list of nodes providing the data
      QList<NodeItem> nodes; nodes.reserve(Ntriple);
      for (size_t i=0; i<Ntriple; i++) {
        nodes.push_back(
              NodeItem(msg.payload.result.triples[i].id,
                       QHostAddress((const Q_IPV6ADDR &) *(msg.payload.result.triples[i].ip)),
                       ntohs(msg.payload.result.triples[i].port)));
      }
      // signal success
      emit valueFound(req->query()->id(), nodes);
      // query done
      delete req->query();
      return;
    }

    // If value was not found -> proceed with returned nodes
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      NodeItem item(id, QHostAddress((const Q_IPV6ADDR &)* (msg.payload.result.triples[i].ip)),
                    ntohs(msg.payload.result.triples[i].port));
      // Add discovered node to buckets
      _buckets.add(id, item.addr(), item.port());
      // Update node list of query
      req->query()->update(item);
    }
  }

  NodeItem next;
  // get next node to query, if there is no next node -> search failed
  if (! req->query()->next(next)) {
    // signal error
    emit valueNotFound(req->query()->id());
    // delete query
    delete req->query();
    // done.
    return;
  }
  // Send next request
  sendFindValue(next, req->query());
}

void
DHT::_processStartStreamResponse(
    const Message &msg, size_t size, StartStreamRequest *req, const QHostAddress &addr, uint16_t port)
{
  // Verify session key
  if (! req->query()->verify(msg.payload.start_stream.pubkey, size-DHT_HASH_SIZE-3)) {
    logError() << "Verification of peer session key failed.";
    delete req->query(); return;
  }
  // Verify fingerprints
  if (!(req->query()->peerId() == req->peedId())) {
    logError() << "Peer fingerprint mismatch: " << req->query()->peerId()
               << " != " << req->peedId();
    delete req->query(); return;
  }

  // Success -> start stream
  if (! req->query()->start(req->cookie(), PeerItem(addr, port), &_socket)) {
    logError() << "Can not start sym. crypt.";
    delete req->query(); return;
  }

  // Stream started: register stream & notify stream handler
  _streams[req->cookie()] = req->query();
  _streamHandler->connectionStarted(req->query());
}

void
DHT::_processPingRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  logDebug() << "Received Ping request from " << addr << ":" << port;
  // simply assemble a pong response including my own ID
  struct Message resp;
  memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
  memcpy(resp.payload.ping.id, _self.id().data(), DHT_HASH_SIZE);
  resp.payload.ping.type = MSG_PING;
  // send
  logDebug() << "Send Ping response to " << addr << ":" << port;
  if(0 > _socket.writeDatagram((char *) &resp, 2*DHT_HASH_SIZE+1, addr, port)) {
    logError() << "Failed to send Ping response to " << addr << ":" << port;
  }
  // Add node to candidate nodes for the bucket table if not known already
  if ((! _buckets.contains(msg.payload.ping.id)) && (10 > _candidates.size())) {
    _candidates.push_back(PeerItem(addr, port));
  }
}

void
DHT::_processFindNodeRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  QList<NodeItem> best;
  _buckets.getNearest(msg.payload.find_node.id, best);

  logDebug() << "Assemble FindNode response:";

  struct Message resp;
  // Assemble response
  memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
  resp.payload.result.success = 0;
  // Determine the number of nodes to reply
  int N = std::min(std::min(DHT_K, best.size()),
                   int(size-1-DHT_HASH_SIZE)/DHT_TRIPLE_SIZE);
  // Add items
  QList<NodeItem>::iterator item = best.begin();
  for (int i = 0; (item!=best.end()) && (i<N); item++, i++) {
    memcpy(resp.payload.result.triples[i].id, item->id().data(), DHT_HASH_SIZE);
    memcpy(resp.payload.result.triples[i].ip, item->addr().toIPv6Address().c, 16);
    resp.payload.result.triples[i].port = htons(item->port());
    logDebug() << " add: " << item->id()
               << "@" << item->addr() << ":" << ntohs(resp.payload.result.triples[i].port);
  }

  // Compute size and send reponse
  size_t resp_size = (1 + DHT_HASH_SIZE + N*DHT_TRIPLE_SIZE);
  _socket.writeDatagram((char *) &resp, resp_size, addr, port);
}

void
DHT::_processFindValueRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  struct Message resp;

  if (_announcements.contains(msg.payload.find_value.id)) {
    QHash<Identifier, AnnouncementItem> &owners
        = _announcements[msg.payload.find_value.id];
    // Assemble response
    memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
    resp.payload.result.success = 1;
    // Determine the number of nodes to reply
    int N = std::min(std::min(DHT_MAX_TRIPLES, owners.size()),
                     int(size-1-DHT_HASH_SIZE)/DHT_TRIPLE_SIZE);
    QHash<Identifier, AnnouncementItem>::iterator item = owners.begin();
    for (int i = 0; (item!=owners.end()) && (i<N); item++, i++) {
      memcpy(resp.payload.result.triples[i].id, item.key().data(), DHT_HASH_SIZE);
      memcpy(resp.payload.result.triples[i].ip, item->addr().toIPv6Address().c, 16);
      resp.payload.result.triples[i].port = htons(item->port());
    }
    // Compute size and send reponse
    size_t resp_size = 1+DHT_HASH_SIZE+N*DHT_TRIPLE_SIZE;
    _socket.writeDatagram((char *) &resp, resp_size, addr, port);
  } else {
    // Get best matching nodes from the buckets
    QList<NodeItem> best;
    _buckets.getNearest(msg.payload.find_node.id, best);
    // Assemble response
    memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
    resp.payload.result.success = 0;
    // Determine the number of nodes to reply
    int N = std::min(std::min(DHT_K, best.size()),
                     int(size-1-DHT_HASH_SIZE)/DHT_TRIPLE_SIZE);
    QList<NodeItem>::iterator item = best.begin();
    for (int i = 0; (item!=best.end()) && (i<N); item++, i++) {
      memcpy(resp.payload.result.triples[i].id, item->id().data(), DHT_HASH_SIZE);
      memcpy(resp.payload.result.triples[i].ip, item->addr().toIPv6Address().c, 16);
      resp.payload.result.triples[i].port = htons(item->port());
    }
    // Compute size and send reponse
    size_t resp_size = 1+DHT_HASH_SIZE+N*DHT_TRIPLE_SIZE;
    _socket.writeDatagram((char *) &resp, resp_size, addr, port);
  }
}

void
DHT::_processAnnounceRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  //
  Identifier value(msg.payload.announce.what);
  // Check if I am closer to the value than any of my nodes in the bucket
  QList<NodeItem> best;
  _buckets.getNearest(value, best);
  if ((best.last().id()-value)>(_self.id()-value)) {
    if (!_announcements.contains(value)) {
      _announcements.insert(value, QHash<Identifier, AnnouncementItem>());
    }
    _announcements[value][msg.payload.announce.who]
        = AnnouncementItem(addr, port);
  }
}

void
DHT::_processStartStreamRequest(const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  logDebug() << "Received StartStream request, service: " << ntohs(msg.payload.start_stream.service);
  // check if a stream handler is installed
  if (0 == _streamHandler) {
    logInfo() << "No stream handler installed -> ignore request";
    return;
  }
  // Request new stream from stream handler
  SecureSocket *stream = 0;
  if (0 == (stream =_streamHandler->newSocket(ntohs(msg.payload.start_stream.service))) ) {
    logInfo() << "Stream handler refuses to create a new stream.";
    return;
  }
  // Verify
  if (! stream->verify(msg.payload.start_stream.pubkey, size-DHT_HASH_SIZE-3)) {
    logError() << "Can not verify stream peer.";
    delete stream; return;
  }
  // check if stream is allowed
  if (! _streamHandler->allowConnection(ntohs(msg.payload.start_stream.service), NodeItem(stream->peerId(), addr, port))) {
    logError() << "Stream recjected by StreamHandler.";
    delete stream; return;
  }

  // Assemble response
  Message resp; int keyLen=0;
  memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
  resp.payload.start_stream.type = MSG_START_STREAM;
  resp.payload.start_stream.service = msg.payload.start_stream.service;
  if (0 > (keyLen = stream->prepare(resp.payload.start_stream.pubkey, DHT_MAX_PUBKEY_SIZE))) {
    logError() << "Can not prepare stream.";
    delete stream; return;
  }

  if (! stream->start(resp.cookie, PeerItem(addr, port), &_socket)) {
    logError() << "Can not finish SecureStream handshake.";
    delete stream; return;
  }

  // compute message size
  keyLen += DHT_HASH_SIZE+3;
  // Send response
  if (keyLen != _socket.writeDatagram((char *)&resp, keyLen, addr, port)) {
    logError() << "Can not send StartStream response";
    delete stream; return;
  }

  logDebug() << "Stream started.";
  // Stream started..
  _streams[resp.cookie] = stream;
  _streamHandler->connectionStarted(stream);
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
    if (MSG_PING == (*req)->type()) {
      logInfo() << "Ping request timeout...";
      // Just ignore
      delete *req;
    } else if (MSG_FIND_NODE == (*req)->type()) {
      logInfo() << "FindNode request timeout...";
      FindNodeQuery *query = static_cast<FindNodeRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> search failed
      if (! query->next(next)) {
        // If the node search is a pending announcement
        if (isPendingAnnouncement(query->id())) {
          // -> send announcement to the best nodes
          QList<NodeItem>::iterator node = query->best().begin();
          for (; node != query->best().end(); node++) {
            sendAnnouncement(*node, query->id());
          }
        } else {
          // if it was a regular node search -> signal error
          emit nodeNotFound(query->id(), query->best());
        }
        // delete request and query
        delete *req; delete query;
      } else {
        // Continue search
        sendFindNode(next, query); delete *req;
      }
    } else if (MSG_FIND_VALUE == (*req)->type()) {
      logInfo() << "FindValue request timeout...";
      FindValueQuery *query = static_cast<FindValueRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> search failed
      if (! query->next(next)) {
        // signal error
        emit valueNotFound(query->id());
        // delete request & query
        delete *req; delete query;
      } else {
        // Send next request
        sendFindValue(next, query);
        delete *req;
      }
    } else if (MSG_START_STREAM == (*req)->type()) {
      logInfo() << "StartStream request timeout...";
      if (_streamHandler) {
        _streamHandler->connectionFailed(
              static_cast<StartStreamRequest *>(*req)->query());
      }
      // delete stream
      delete static_cast<StartStreamRequest *>(*req)->query();
      delete *req;
    }
  }
}

void
DHT::_onCheckNodeTimeout() {
  // Collect old nodes from buckets
  QList<NodeItem> oldNodes;
  _buckets.getOlderThan(15*60, oldNodes);
  // send a ping to all of them
  QList<NodeItem>::iterator node = oldNodes.begin();
  for (; node != oldNodes.end(); node++) {
    ping(node->addr(), node->port());
  }
  // Remove dead nodes from the buckets
  _buckets.removeOlderThan(20*60);
  // Send pings to candidates
  while (_candidates.size()) {
    ping(_candidates.first());
    _candidates.pop_front();
  }
}

void
DHT::_onCheckAnnouncementTimeout() {
  // Check announcements I store for others
  QHash<Identifier, QHash<Identifier, AnnouncementItem> >::iterator entry = _announcements.begin();
  while (entry != _announcements.end()) {
    QHash<Identifier, AnnouncementItem>::iterator ann = entry->begin();
    while (ann != entry->end()) {
      if (ann->olderThan(30*60)) {
        ann = entry->erase(ann);
      } else {
        ann++;
      }
    }
    if (entry->empty()) {
      entry = _announcements.erase(entry);
    } else {
      entry++;
    }
  }

  // Check announcements I have to make (data that I provide).
  QHash<Identifier, QDateTime>::iterator item = _announcedData.begin();
  for (; item != _announcedData.end(); item++) {
    // re-announce the data every 20minutes
    if (item->addSecs(20*60) < QDateTime::currentDateTime()) {
      announce(item.key());
    }
  }
}

void
DHT::_onUpdateStatistics() {
  _inRate = (_bytesReceived-_lastBytesReceived)/5;
  _lastBytesReceived = _bytesReceived;
  _outRate = (_bytesSend - _lastBytesSend)/5;
  _lastBytesSend = _bytesSend;
}
