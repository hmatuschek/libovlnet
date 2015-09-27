#include "dht.h"
#include "crypto.h"
#include "dht_config.h"

#include <QHostInfo>
#include <netinet/in.h>


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
  char     id[DHT_HASH_SIZE];
  /** The IP of the node. */
  uint32_t  ip;
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
      char    id[DHT_HASH_SIZE];
    } find_node;

    struct __attribute__((packed)) {
      uint8_t type; // == FIND_VALUE
      char    id[DHT_HASH_SIZE];
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
  StartStreamRequest(uint16_t service, const Identifier &peer, SecureStream *stream);

  inline SecureStream *query() const { return _stream; }
  inline uint16_t service() const { return _service; }
  inline const Identifier &peedId() const { return _peer; }

protected:
  uint16_t _service;
  Identifier _peer;
  SecureStream *_stream;
};


/* ******************************************************************************************** *
 * Implementation of Identifier
 * ******************************************************************************************** */
Identifier::Identifier()
  : QByteArray(DHT_HASH_SIZE, 0)
{
  for (int i=0; i<DHT_HASH_SIZE; i++) {
    (*this)[i] = qrand() % 0xff;
  }
}

Identifier::Identifier(const char *id)
  : QByteArray(id, DHT_HASH_SIZE)
{
  // pass...
}

Identifier::Identifier(const QByteArray &id)
  : QByteArray(id)
{
  // pass...
}

Identifier::Identifier(const Identifier &other)
  : QByteArray(other)
{
  // pass...
}

Identifier &
Identifier::operator=(const Identifier &other) {
  QByteArray::operator=(other);
  return *this;
}

bool
Identifier::operator==(const Identifier &other) const {
  for (size_t i=0; i<DHT_HASH_SIZE; i++) {
    if (this->at(i) ^ other.at(i)) { return false; }
  }
  return true;
}

Distance
Identifier::operator-(const Identifier &other) const {
  return Distance(*this, other);
}


/* ******************************************************************************************** *
 * Implementation of Distance
 * ******************************************************************************************** */
Distance::Distance(const Identifier &a, const Identifier &b)
  : QByteArray(DHT_HASH_SIZE, 0xff)
{
  for (uint i=0; i<DHT_HASH_SIZE; i++) {
    (*this)[i] = (a[i] ^ b[i]);
  }
}

Distance::Distance(const Distance &distance)
  : QByteArray(distance)
{
  // pass...
}

bool
Distance::bit(size_t idx) const {
  size_t byte = idx/8, bit  = (7-(idx%8));
  return (1 == (this->at(byte) >> bit));
}

size_t
Distance::leadingBit() const {
  for (size_t i=0; i<(8*DHT_HASH_SIZE); i++) {
    if (bit(i)) { return i; }
  }
  return 8*DHT_HASH_SIZE;
}

/* ******************************************************************************************** *
 * Implementation of PeerItem
 * ******************************************************************************************** */
PeerItem::PeerItem()
  : _addr(), _port(0)
{
  // pass...
}

PeerItem::PeerItem(const QHostAddress &addr, uint16_t port)
  : _addr(addr), _port(port)
{
  // pass...
}

PeerItem::PeerItem(const PeerItem &other)
  : _addr(other._addr), _port(other._port)
{
  // pass...
}

PeerItem &
PeerItem::operator =(const PeerItem &other) {
  _addr = other._addr;
  _port = other._port;
  return *this;
}

const QHostAddress &
PeerItem::addr() const {
  return _addr;
}

uint16_t
PeerItem::port() const {
  return _port;
}


/* ******************************************************************************************** *
 * Implementation of NodeItem
 * ******************************************************************************************** */
NodeItem::NodeItem()
  : PeerItem(), _id()
{
  // pass...
}

NodeItem::NodeItem(const Identifier &id, const QHostAddress &addr, uint16_t port)
  : PeerItem(addr, port), _id(id)
{
  // pass...
}

NodeItem::NodeItem(const Identifier &id, const PeerItem &peer)
  : PeerItem(peer), _id(id)
{
  // pass...
}

NodeItem::NodeItem(const NodeItem &other)
  : PeerItem(other), _id(other._id)
{
  // pass...
}

NodeItem &
NodeItem::operator =(const NodeItem &other) {
  PeerItem::operator =(other);
  _id   = other._id;
  return *this;
}

const Identifier &
NodeItem::id() const {
  return _id;
}


/* ******************************************************************************************** *
 * Implementation of AnnouncementItem
 * ******************************************************************************************** */
AnnouncementItem::AnnouncementItem()
  : PeerItem(), _timestamp()
{
  // pass...
}

AnnouncementItem::AnnouncementItem(const QHostAddress &addr, uint16_t port)
  : PeerItem(addr, port), _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

AnnouncementItem::AnnouncementItem(const AnnouncementItem &other)
  : PeerItem(other), _timestamp(other._timestamp)
{
  // pass...
}

AnnouncementItem &
AnnouncementItem::operator =(const AnnouncementItem &other) {
  PeerItem::operator =(other);
  _timestamp = other._timestamp;
  return *this;
}

bool
AnnouncementItem::olderThan(size_t seconds) {
  return (_timestamp.addSecs(seconds)<QDateTime::currentDateTime());
}


/* ******************************************************************************************** *
 * Implementation of Bucket::Item
 * ******************************************************************************************** */
Bucket::Item::Item()
  : _prefix(0), _peer(QHostAddress(), 0), _lastSeen()
{
  // pass...
}

Bucket::Item::Item(const QHostAddress &addr, uint16_t port, size_t prefix)
  : _prefix(prefix), _peer(addr, port), _lastSeen(QDateTime::currentDateTime())
{
  // pass...
}

Bucket::Item::Item(const Item &other)
  : _prefix(other._prefix), _peer(other._peer), _lastSeen(other._lastSeen)
{
  // pass...
}

Bucket::Item &
Bucket::Item::operator =(const Item &other) {
  _prefix   = other._prefix;
  _peer     = other._peer;
  _lastSeen = other._lastSeen;
  return *this;
}

size_t
Bucket::Item::prefix() const {
  return _prefix;
}

const PeerItem &
Bucket::Item::peer() const {
  return _peer;
}

const QHostAddress &
Bucket::Item::addr() const {
  return _peer.addr();
}

uint16_t
Bucket::Item::port() const {
  return _peer.port();
}

const QDateTime &
Bucket::Item::lastSeen() const {
  return _lastSeen;
}


/* ******************************************************************************************** *
 * Implementation of Bucket
 * ******************************************************************************************** */
Bucket::Bucket(const Identifier &self)
  : _self(self), _maxSize(DHT_K), _prefix(0), _triples()
{
  // pass...
}

Bucket::Bucket(const Bucket &other)
  : _self(other._self), _maxSize(other._maxSize), _prefix(other._prefix), _triples(other._triples)
{
  // pass...
}

bool
Bucket::full() const {
  return int(_maxSize)==_triples.size();
}

size_t
Bucket::numNodes() const {
  return _triples.size();
}

void
Bucket::nodes(QList<NodeItem> &lst) const {
  QHash<Identifier, Item>::const_iterator item = _triples.begin();
  for (; item != _triples.end(); item++) {
    lst.push_back(NodeItem(item.key(), item->addr(), item->port()));
  }
}

bool
Bucket::contains(const Identifier &id) const {
  return _triples.contains(id);
}

void
Bucket::add(const Identifier &id, const Item &item) {
  _triples[id] = item;
}

void
Bucket::add(const Identifier &id, const QHostAddress &addr, uint16_t port) {
  if (contains(id) || (!full())) {
    _triples[id] = Item(addr, port, (id-_self).leadingBit());
  }
}

size_t
Bucket::prefix() const {
  return _prefix;
}

void
Bucket::split(Bucket &newBucket) {
  // Add all items from this bucket to the new one, which have a higher
  // prefix than the prefix of this bucket
  QHash<Identifier, Item>::iterator item = _triples.begin();
  for (; item != _triples.end(); item++) {
    if (item->prefix() > _prefix) {
      newBucket.add(item.key(), *item);
    }
  }
  item = newBucket._triples.begin();
  for (; item != newBucket._triples.end(); item++) {
    _triples.remove(item.key());
  }
}

void
Bucket::getNearest(const Identifier &id, QList<NodeItem> &best) const {
  QHash<Identifier, Item>::const_iterator item = _triples.begin();
  for (; item != _triples.end(); item++) {
    // Perform an "insort" into best list
    Distance d = id - item.key();
    QList<NodeItem>::iterator node = best.begin();
    while ((node != best.end()) && (d>=(id-node->id()))) { node++; }
    best.insert(node, NodeItem(item.key(), item->addr(), item->port()));
    while (best.size() > DHT_K) { best.pop_back(); }
  }
}

void
Bucket::getOlderThan(size_t age, QList<NodeItem> &nodes) const {
  QHash<Identifier, Item>::const_iterator item = _triples.begin();
  for (; item != _triples.end(); item++) {
    if (item->olderThan(age)) {
      nodes.append(NodeItem(item.key(), item->addr(), item->port()));
    }
  }
}

void
Bucket::removeOlderThan(size_t age) {
  QHash<Identifier, Item>::iterator item = _triples.begin();
  while (item != _triples.end()) {
    if (item->olderThan(age)) {
      qDebug() << "Lost contact to" << item.key()
               << "@" << item->addr() << ":" << item->port();
      item = _triples.erase(item);
    } else {
      item++;
    }
  }
}


/* ******************************************************************************************** *
 * Implementation of Buckets
 * ******************************************************************************************** */
Buckets::Buckets(const Identifier &self)
  : _self(self), _buckets()
{
  _buckets.reserve(8*DHT_HASH_SIZE);
}

bool
Buckets::empty() const {
  return 0 == _buckets.size();
}

size_t
Buckets::numNodes() const {
  size_t count = 0;
  QList<Bucket>::const_iterator item = _buckets.begin();
  for (; item != _buckets.end(); item++) {
    count += item->numNodes();
  }
  return count;
}

void
Buckets::nodes(QList<NodeItem> &lst) const {
  QList<Bucket>::const_iterator bucket = _buckets.begin();
  for (; bucket != _buckets.end(); bucket++) {
    bucket->nodes(lst);
  }
}

bool
Buckets::contains(const Identifier &id) const {
  QList<Bucket>::const_iterator bucket = _buckets.begin();
  for (; bucket != _buckets.end(); bucket++) {
    if (bucket->contains(id)) { return true; }
  }
  return false;
}

void
Buckets::add(const Identifier &id, const QHostAddress &addr, uint16_t port) {
  // Do not add myself
  if (id == _self) { return; }

  // If there are no buckets -> create one.
  if (empty()) {
    _buckets.append(Bucket(_self));
    _buckets.back().add(id, addr, port);
    return;
  }

  // Find the bucket, the item belongs to
  QList<Bucket>::iterator bucket = index(id);

  if (bucket->contains(id) || (! bucket->full())) {
    // If the bucket contains the item already or the bucket is not full
    //  -> update
    bucket->add(id, addr, port);
  } else {
    // If the item is new to the bucket and if the bucket is full
    //  -> check if it can be splitted
    QList<Bucket>::iterator next = bucket; next++;
    if (next==_buckets.end()) {
      Bucket newBucket(_self);
      bucket->split(newBucket);
      _buckets.insert(next, newBucket);
      size_t prefix = (id-_self).leadingBit();
      if (next->prefix() == prefix) { next->add(id, addr, port); }
      else { this->add(id, addr, port); }
    }
  }
}

void
Buckets::getNearest(const Identifier &id, QList<NodeItem> &best) const {
  QList<Bucket>::const_iterator bucket = _buckets.begin();
  for (; bucket != _buckets.end(); bucket++) {
    bucket->getNearest(id, best);
  }
}

void
Buckets::getOlderThan(size_t seconds, QList<NodeItem> &nodes) const {
  QList<Bucket>::const_iterator bucket = _buckets.begin();
  for (; bucket != _buckets.end(); bucket++) {
    bucket->getOlderThan(seconds, nodes);
  }
}

void
Buckets::removeOlderThan(size_t seconds) {
  QList<Bucket>::iterator bucket = _buckets.begin();
  for (; bucket != _buckets.end(); bucket++) {
    bucket->removeOlderThan(seconds);
  }
}

QList<Bucket>::iterator
Buckets::index(const Identifier &id) {
  size_t prefix = (id-_self).leadingBit();
  if (2 > _buckets.size()) { return _buckets.begin(); }
  QList<Bucket>::iterator current = _buckets.begin();
  QList<Bucket>::iterator next = _buckets.begin(); next++;
  for (; next < _buckets.end(); current++, next++) {
    if (current->prefix() == prefix) { return current; }
    if (next->prefix() > prefix) { return current; }
  }
  return _buckets.end()--;
}


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
  qDebug() << "Update search list with" << node.id()
           << "@" << node.addr() << ":" << node.port();
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

StartStreamRequest::StartStreamRequest(uint16_t service, const Identifier &peer, SecureStream *stream)
  : Request(MSG_START_STREAM), _service(service), _peer(peer), _stream(stream)
{
  // pass...
}


/* ******************************************************************************************** *
 * Implementation of DHT
 * ******************************************************************************************** */
DHT::DHT(const Identifier &id, StreamHandler *streamHandler,
         const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _self(id), _socket(), _bytesReceived(0), _lastBytesReceived(0), _inRate(0),
    _bytesSend(0), _lastBytesSend(0), _outRate(0), _buckets(_self),
    _streamHandler(streamHandler), _streams(),
    _requestTimer(), _nodeTimer(), _announcementTimer(), _statisticsTimer()
{
  qDebug() << "Start node #" << id << "at" << addr << ":" << port;

  if (!_socket.bind(addr, port)) {
    qDebug() << "Cannot bind to port" << addr << ":" << port;
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
  ping(info.addresses().front(), port);
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
  memcpy(msg.payload.ping.id, _self.data(), DHT_HASH_SIZE);
  msg.payload.ping.type = MSG_PING;
  qDebug() << "Send ping to" << addr << ":" << port;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, 2*DHT_HASH_SIZE+1, addr, port)) {
    qDebug() << "Failed to send ping to" << addr << ":" << port;
  }
}

void
DHT::findNode(const Identifier &id) {
  qDebug() << "Search for node" << id;
  // Create a query instance
  FindNodeQuery *query = new FindNodeQuery(id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    qDebug() << "Can not find node" << id << ". Buckets empty.";
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
  qDebug() << "Search for value" << id;
  // Create a query instance
  FindValueQuery *query = new FindValueQuery(id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    qDebug() << "Can not find value" << id << ". Buckets empty.";
    delete query; emit valueNotFound(id);
  } else {
    sendFindValue(next, query);
  }
}

void
DHT::announce(const Identifier &id) {
  qDebug() << "Announce data " << id << "to the world.";
  _announcedData[id] = QDateTime();
  // lets search for the nodes closest to the data id
  findNode(id);
}

bool
DHT::startStream(uint16_t service, const NodeItem &node) {
  if (0 == _streamHandler) { return false; }
  qDebug() << "Send start stream to" << node.id() << "@" << node.addr() << ":" << node.port();
  SecureStream *stream = _streamHandler->newStream(service);
  if (! stream) { return false; }
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
DHT::closeStream(const Identifier &id) {
  _streams.remove(id);
}

const Identifier &
DHT::id() const {
  return _self;
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
  qDebug() << "Send FindNode request to" << to.id()
           << "@" << to.addr() << ":" << to.port();
  // Construct request item
  FindNodeRequest *req = new FindNodeRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  struct Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  msg.payload.find_node.type = MSG_FIND_NODE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  if(0 > _socket.writeDatagram((char *)&msg, 2*DHT_HASH_SIZE+1, to.addr(), to.port())) {
    qDebug() << "Failed to send FindNode request to" << to.id()
             << "@" << to.addr() << ":" << to.port();
  }
}

void
DHT::sendFindValue(const NodeItem &to, FindValueQuery *query) {
  qDebug() << "Send FindValue request to" << to.id()
           << "@" << to.addr() << ":" << to.port();
  // Construct request item
  FindValueRequest *req = new FindValueRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  struct Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  msg.payload.find_node.type = MSG_FIND_VALUE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  if (0 > _socket.writeDatagram((char *)&msg, 2*DHT_HASH_SIZE+1, to.addr(), to.port())) {
    qDebug() << "Failed to send FindNode request to" << to.id()
             << "@" << to.addr() << ":" << to.port();
  }
}

bool
DHT::isPendingAnnouncement(const Identifier &id) const {
  return _announcedData.contains(id);
}

void
DHT::sendAnnouncement(const NodeItem &to, const Identifier &what) {
  qDebug() << "Send Announcement of" << what << "to" << to.id()
           << "@" << to.addr() << ":" << to.port();
  // Assemble & send message
  struct Message msg;
  memcpy(msg.cookie, Identifier().data(), DHT_HASH_SIZE);
  memcpy(msg.payload.announce.what, what.data(), DHT_HASH_SIZE);
  memcpy(msg.payload.announce.who, _self.data(), DHT_HASH_SIZE);
  msg.payload.announce.type = MSG_ANNOUNCE;
  if (0 > _socket.writeDatagram((char *)&msg, 3*DHT_HASH_SIZE+1, to.addr(), to.port())) {
    qDebug() << "Failed to send Announce request to" << to.id()
             << "@" << to.addr() << ":" << to.port();
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
      qDebug() << "Invalid UDP packet received from" << addr << ":" << port;
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
          qDebug() << "Unknown response from " << addr << ":" << port;
        }
        delete item;
      } else {
        // Message is likely a request
        if ((size == (2*DHT_HASH_SIZE+1)) && (MSG_PING == msg.payload.ping.type)){
          _processPingRequest(msg, size, addr, port);
        } else if ((size == (2*DHT_HASH_SIZE+1)) && (MSG_FIND_NODE == msg.payload.find_node.type)) {
          _processFindNodeRequest(msg, size, addr, port);
        } else if ((size == (2*DHT_HASH_SIZE+1)) && (MSG_FIND_VALUE == msg.payload.find_value.type)) {
          _processFindValueRequest(msg, size, addr, port);
        } else if ((size == (3*DHT_HASH_SIZE+1)) && (MSG_ANNOUNCE == msg.payload.announce.type)) {
          _processAnnounceRequest(msg, size, addr, port);
        } else if ((size > (DHT_HASH_SIZE+3)) && (MSG_START_STREAM == msg.payload.announce.type)) {
          _processStartStreamRequest(msg, size, addr, port);
        } else {
          qDebug() << "Unknown request from " << addr << ":" << port;
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
  qDebug() << "Received Ping response from " << addr << ":" << port;
  // sinal success
  emit nodeReachable(NodeItem(msg.payload.ping.id, addr, port));
  // If the buckets are empty -> we are likely bootstrapping
  bool bootstrapping = _buckets.empty();
  // Given that this is a ping response -> add the node to the corresponding
  // bucket if space is left
  _buckets.add(msg.payload.ping.id, addr, port);
  if (bootstrapping) {
    qDebug() << "Still boot strapping: Search for myself.";
    findNode(_self);
  }
}

void
DHT::_processFindNodeResponse(
    const struct Message &msg, size_t size, FindNodeRequest *req, const QHostAddress &addr, uint16_t port)
{
  qDebug() << "Received FindNode response from " << addr << ":" << port;
  // payload length must be a multiple of triple length
  if ( 0 != ((size-DHT_HASH_SIZE-1)%DHT_TRIPLE_SIZE) ) {
    qDebug() << "Received a malformed FIND_NODE response from"
             << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE;
    qDebug() << "Received" << Ntriple << "nodes from"  << addr << ":" << port;
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      NodeItem item(id, QHostAddress(ntohl(msg.payload.result.triples[i].ip)),
                    ntohs(msg.payload.result.triples[i].port));
      qDebug() << " got: " << item.id() << "@" << item.addr() << ":" << ntohs(msg.payload.result.triples[i].port);
      // Add discovered node to buckets
      _buckets.add(id, item.addr(), item.port());
      // Update node list of query
      req->query()->update(item);
    }

    // If the node was found -> signal success
    if (req->query()->found()) {
      qDebug() << "Found node" << req->query()->first().id()
               << "@" << req->query()->first().addr() << ":" << req->query()->first().port();
      // Signal node found
      emit nodeFound(req->query()->first());
      // delete query
      delete req->query();
      // done
      return;
    }
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
      qDebug() << "Node" << req->query()->id() << "not found.";
      // if it was a regular node search -> signal error
      emit nodeNotFound(req->query()->id(), req->query()->best());
    }
    // delete query
    delete req->query();
    // done.
    return;
  }

  qDebug() << "Node" << req->query()->id() << "not found yet -> continue with"
           << next.id() << "@" << next.addr() << ":" << next.port();
  // Send next request
  sendFindNode(next, req->query());
}

void
DHT::_processFindValueResponse(
    const struct Message &msg, size_t size, FindValueRequest *req, const QHostAddress &addr, uint16_t port)
{
  // payload length must be a multiple of triple length
  if ( 0 != ((size-DHT_HASH_SIZE-1)%DHT_TRIPLE_SIZE) ) {
    qDebug() << "Received a malformed FIND_NODE response from"
             << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE;

    // If queried node has value
    if (msg.payload.result.success) {
      // Get list of nodes providing the data
      QList<NodeItem> nodes; nodes.reserve(Ntriple);
      for (size_t i=0; i<Ntriple; i++) {
        nodes.push_back(NodeItem(msg.payload.result.triples[i].id,
                                 QHostAddress(ntohl(msg.payload.result.triples[i].ip)),
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
      NodeItem item(id, QHostAddress(ntohl(msg.payload.result.triples[i].ip)),
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
    qDebug() << "Verification of peer session key failed.";
    delete req->query(); return;
  }
  // Verify fingerprints
  if (!(req->query()->peerId() == req->peedId())) {
    qDebug() << "Peer fingerprint mismatch!";
    delete req->query(); return;
  }

  // Success -> start stream
  if (! req->query()->start(req->cookie(), PeerItem(addr, port), &_socket)) {
    qDebug() << "Can not start sym. crypt.";
    delete req->query(); return;
  }

  // Stream started: register stream & notify stream handler
  _streams[req->cookie()] = req->query();
  _streamHandler->streamStarted(req->query());
}

void
DHT::_processPingRequest(
    const struct Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  qDebug() << "Received Ping request from" << addr << ":" << port;
  // simply assemble a pong response including my own ID
  struct Message resp;
  memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
  memcpy(resp.payload.ping.id, _self.data(), DHT_HASH_SIZE);
  resp.payload.ping.type = MSG_PING;
  // send
  qDebug() << "Send Ping response to" << addr << ":" << port;
  if(0 > _socket.writeDatagram((char *) &resp, 2*DHT_HASH_SIZE+1, addr, port)) {
    qDebug() << "Failed to send Ping response to" << addr << ":" << port;
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

  qDebug() << "Assemble FindNode response:";

  struct Message resp;
  // Assemble response
  memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
  resp.payload.result.success = 0;
  // Add items
  QList<NodeItem>::iterator item = best.begin();
  for (int i = 0; (item!=best.end()) && (i<DHT_K); item++, i++) {
    memcpy(resp.payload.result.triples[i].id, item->id().data(), DHT_HASH_SIZE);
    resp.payload.result.triples[i].ip = htonl(item->addr().toIPv4Address());
    resp.payload.result.triples[i].port = htons(item->port());
    qDebug() << " add: " << item->id()
             << "@" << item->addr() << ":" << ntohs(resp.payload.result.triples[i].port);
  }

  // Compute size and send reponse
  size_t resp_size = (1 + DHT_HASH_SIZE + std::min(best.size(), DHT_K)*DHT_TRIPLE_SIZE);
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
    QHash<Identifier, AnnouncementItem>::iterator item = owners.begin();
    for (int i = 0; (item!=owners.end()) && (i<DHT_MAX_TRIPLES); item++, i++) {
      memcpy(resp.payload.result.triples[i].id, item.key().data(), DHT_HASH_SIZE);
      resp.payload.result.triples[i].ip = htonl(item->addr().toIPv4Address());
      resp.payload.result.triples[i].port = htons(item->port());
    }
    // Compute size and send reponse
    size_t resp_size = 1+DHT_HASH_SIZE+std::min(owners.size(), DHT_K)*DHT_TRIPLE_SIZE;
    _socket.writeDatagram((char *) &resp, resp_size, addr, port);
  } else {
    // Get best matching nodes from the buckets
    QList<NodeItem> best;
    _buckets.getNearest(msg.payload.find_node.id, best);
    // Assemble response
    memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
    resp.payload.result.success = 0;
    QList<NodeItem>::iterator item = best.begin();
    for (int i = 0; (item!=best.end()) && (i<DHT_K); item++, i++) {
      memcpy(resp.payload.result.triples[i].id, item->id().data(), DHT_HASH_SIZE);
      resp.payload.result.triples[i].ip = htonl(item->addr().toIPv4Address());
      resp.payload.result.triples[i].port = htons(item->port());
    }
    // Compute size and send reponse
    size_t resp_size = 1+DHT_HASH_SIZE+std::min(best.size(), DHT_K)*DHT_TRIPLE_SIZE;
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
  if ((best.last().id()-value)>(_self-value)) {
    if (!_announcements.contains(value)) {
      _announcements.insert(value, QHash<Identifier, AnnouncementItem>());
    }
    _announcements[value][msg.payload.announce.who]
        = AnnouncementItem(addr, port);
  }
}

void
DHT::_processStartStreamRequest(const Message &msg, size_t size, const QHostAddress &addr, uint16_t port) {
  if (0 == _streamHandler) { return; }
  // Request new stream
  SecureStream *stream = 0;
  if (0 == (stream =_streamHandler->newStream(ntohs(msg.payload.start_stream.service))) )
    return;
  // Verify
  if (! stream->verify(msg.payload.start_stream.pubkey, size-DHT_HASH_SIZE-3)) {
    qDebug() << "Can not verify stream peer.";
    delete stream; return;
  }
  // check if stream is allowed
  if (! _streamHandler->allowStream(ntohs(msg.payload.start_stream.service), NodeItem(stream->peerId(), addr, port))) {
    qDebug() << "Stream recjected by StreamHandler";
    delete stream; return;
  }

  // Assemble response
  Message resp; int keyLen=0;
  memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
  resp.payload.start_stream.type = MSG_START_STREAM;
  resp.payload.start_stream.service = msg.payload.start_stream.service;
  if (0 > (keyLen = stream->prepare(resp.payload.start_stream.pubkey, DHT_MAX_PUBKEY_SIZE))) {
    qDebug() << "Can not prepare stream.";
    delete stream; return;
  }

  if (! stream->start(resp.cookie, PeerItem(addr, port), &_socket)) {
    qDebug() << "Can not finish SecureStream handshake.";
    delete stream; return;
  }

  // compute message size
  keyLen += DHT_HASH_SIZE+3;
  // Send response
  if (keyLen != _socket.writeDatagram((char *)&resp, keyLen, addr, port)) {
    qDebug() << "Can not send StartStream response";
    delete stream; return;
  }

  // Stream started..
  _streams[resp.cookie] = stream;
  _streamHandler->streamStarted(stream);
}

void
DHT::_onCheckRequestTimeout() {
  QList<Request *> deadRequests;
  QHash<Identifier, Request *>::iterator item = _pendingRequests.begin();
  for (; item != _pendingRequests.end(); item++) {
    if ((*item)->olderThan(2000)) {
      deadRequests.append(*item);
    }
  }

  // Process dead requests, dispatch by type
  QList<Request *>::iterator req = deadRequests.begin();
  for (; req != deadRequests.end(); req++) {
    _pendingRequests.remove((*req)->cookie());
    if (MSG_PING == (*req)->type()) {
      qDebug() << "Ping request timeout...";
      // Just ignore
      delete *req;
    } else if (MSG_FIND_NODE == (*req)->type()) {
      qDebug() << "FindNode request timeout...";
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
        // delete query
        delete query;
      }
      // Send next request
      sendFindNode(next, query); delete *req;
    } else if (MSG_FIND_VALUE == (*req)->type()) {
      qDebug() << "FindValue request timeout...";
      FindValueQuery *query = static_cast<FindValueRequest *>(*req)->query();
      // Get next node to query
      NodeItem next;
      // get next node to query, if there is no next node -> search failed
      if (! query->next(next)) {
        // signal error
        emit valueNotFound(query->id());
        // delete query
        delete query;
      }
      // Send next request
      sendFindValue(next, query); delete *req;
    } else if (MSG_START_STREAM == (*req)->type()) {
      qDebug() << "StartStream request timeout...";
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
    ping(_candidates.first()); _candidates.pop_front();
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
