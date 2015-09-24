#include "dht.h"
#include <netinet/in.h>


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
  size_t byte = idx/8, bit  = (7-idx%8);
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
  _addr = other.addr();
  _port = other.port();
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
  _id   = other.id();
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
Bucket::Bucket(const Identifier &self, size_t size)
  : _self(self), _maxSize(size), _prefix(0), _triples()
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
Request::Request(Message::Type type)
  : _type(type), _cookie(), _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

PingRequest::PingRequest()
  : Request(Message::PING)
{
  // pass...
}

FindNodeRequest::FindNodeRequest(FindNodeQuery *query)
  : Request(Message::FIND_NODE), _findNodeQuery(query)
{
  // pass...
}

FindValueRequest::FindValueRequest(FindValueQuery *query)
  : Request(Message::FIND_VALUE), _findValueQuery(query)
{
  // pass...
}


/* ******************************************************************************************** *
 * Implementation of Node
 * ******************************************************************************************** */
Node::Node(const Identifier &id, const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _self(id), _socket(), _buckets(_self), _requestTimer(), _nodeTimer(),
    _announcementTimer()
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

  QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  QObject::connect(&_requestTimer, SIGNAL(timeout()), this, SLOT(_onCheckRequestTimeout()));
  QObject::connect(&_nodeTimer, SIGNAL(timeout()), this, SLOT(_onCheckNodeTimeout()));
  QObject::connect(&_announcementTimer, SIGNAL(timeout()), this, SLOT(_onCheckAnnouncementTimeout()));

  _requestTimer.start();
  _nodeTimer.start();
  _announcementTimer.start();
}

Node::~Node() {
  // pass...
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
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  memcpy(msg.payload.ping.id, _self.data(), DHT_HASH_SIZE);
  msg.payload.ping.type = Message::PING;
  qDebug() << "Send ping to" << addr << ":" << port;
  // send it
  if(0 > _socket.writeDatagram((char *) &msg, 2*DHT_HASH_SIZE+1, addr, port)) {
    qDebug() << "Failed to send ping to" << addr << ":" << port;
  }
}

void
Node::findNode(const Identifier &id) {
  qDebug() << "Search for node" << id;
  // Create a query instance
  FindNodeQuery *query = new FindNodeQuery(id);
  // Collect DHT_K nearest nodes
  _buckets.getNearest(id, query->best());
  // Send request to the first element in the list
  NodeItem next;
  if (! query->next(next)) {
    qDebug() << "Can not find node" << id << ". Buckets empty.";
    delete query;
    // Emmit signal if failiour is not a pending announcement
    if (! isPendingAnnouncement(id)) { emit nodeNotFound(id); }
  } else {
    sendFindNode(next, query);
  }
}

void
Node::findValue(const Identifier &id) {
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
Node::announce(const Identifier &id) {
  qDebug() << "Announce data " << id << "to the world.";
  _announcedData[id] = QDateTime();
  // lets search for the nodes closest to the data id
  findNode(id);
}

QIODevice *
Node::data(const Identifier &id) {
  return 0;
}


/*
 * Implementation of internal used methods.
 */
void
Node::sendFindNode(const NodeItem &to, FindNodeQuery *query) {
  qDebug() << "Send FindNode request to" << to.id()
           << "@" << to.addr() << ":" << to.port();
  // Construct request item
  FindNodeRequest *req = new FindNodeRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  msg.payload.find_node.type = Message::FIND_NODE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  if(0 > _socket.writeDatagram((char *)&msg, 2*DHT_HASH_SIZE+1, to.addr(), to.port())) {
    qDebug() << "Failed to send FindNode request to" << to.id()
             << "@" << to.addr() << ":" << to.port();
  }
}

void
Node::sendFindValue(const NodeItem &to, FindValueQuery *query) {
  qDebug() << "Send FindValue request to" << to.id()
           << "@" << to.addr() << ":" << to.port();
  // Construct request item
  FindValueRequest *req = new FindValueRequest(query);
  // Queue request
  _pendingRequests.insert(req->cookie(), req);
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, req->cookie().data(), DHT_HASH_SIZE);
  msg.payload.find_node.type = Message::FIND_VALUE;
  memcpy(msg.payload.find_node.id, query->id().data(), DHT_HASH_SIZE);
  if (0 > _socket.writeDatagram((char *)&msg, 2*DHT_HASH_SIZE+1, to.addr(), to.port())) {
    qDebug() << "Failed to send FindNode request to" << to.id()
             << "@" << to.addr() << ":" << to.port();
  }
}

bool
Node::isPendingAnnouncement(const Identifier &id) const {
  return _announcedData.contains(id);
}

void
Node::sendAnnouncement(const NodeItem &to, const Identifier &what) {
  qDebug() << "Send Announcement of" << what << "to" << to.id()
           << "@" << to.addr() << ":" << to.port();
  // Assemble & send message
  Message msg;
  memcpy(msg.cookie, Identifier().data(), DHT_HASH_SIZE);
  memcpy(msg.payload.announce.what, what.data(), DHT_HASH_SIZE);
  memcpy(msg.payload.announce.who, _self.data(), DHT_HASH_SIZE);
  msg.payload.announce.type = Message::ANNOUNCE;
  if (0 > _socket.writeDatagram((char *)&msg, 3*DHT_HASH_SIZE+1, to.addr(), to.port())) {
    qDebug() << "Failed to send Announce request to" << to.id()
             << "@" << to.addr() << ":" << to.port();
  }
}

void
Node::_onReadyRead() {
  while (_socket.hasPendingDatagrams()) {
    // check datagram size
    if ( (_socket.pendingDatagramSize() > DHT_MAX_MESSAGE_SIZE) ||
         (_socket.pendingDatagramSize() < DHT_MIN_MESSAGE_SIZE)) {
      QHostAddress addr; uint16_t port;
      // Cannot be a vaild message -> drop it
      _socket.readDatagram(0, 0, &addr, &port);
      qDebug() << "Invalid UDP packet received from" << addr << ":" << port;
    } else {
      // Read message
      Message msg; QHostAddress addr; uint16_t port;
      int64_t size = _socket.readDatagram((char *) &msg, DHT_MAX_MESSAGE_SIZE, &addr, &port);

      Identifier cookie(msg.cookie);
      if (_pendingRequests.contains(cookie)) {
        // Message is a response -> dispatch by type from table
        Request *item = _pendingRequests[cookie];
        _pendingRequests.remove(cookie);
        if (Message::PING == item->type()) {
          _processPingResponse(msg, size, static_cast<PingRequest *>(item), addr, port);
        } else if (Message::FIND_NODE == item->type()) {
          _processFindNodeResponse(msg, size, static_cast<FindNodeRequest *>(item), addr, port);
        } else if (Message::FIND_VALUE == item->type()) {
          _processFindValueResponse(msg, size, static_cast<FindValueRequest *>(item), addr, port);
        } else {
          qDebug() << "Unknown response from " << addr << ":" << port;
        }
        delete item;
      } else {
        // Message is likely a request
        if ((size == (2*DHT_HASH_SIZE+1)) && (Message::PING == msg.payload.ping.type)){
          _processPingRequest(msg, size, addr, port);
        } else if ((size == (2*DHT_HASH_SIZE+1)) && (Message::FIND_NODE == msg.payload.find_node.type)) {
          _processFindNodeRequest(msg, size, addr, port);
        } else if ((size == (2*DHT_HASH_SIZE+1)) && (Message::FIND_VALUE == msg.payload.find_value.type)) {
          _processFindValueRequest(msg, size, addr, port);
        } else if ((size == (3*DHT_HASH_SIZE+1)) && (Message::ANNOUNCE == msg.payload.announce.type)) {
          _processAnnounceRequest(msg, size, addr, port);
        } else {
          qDebug() << "Unknown request from " << addr << ":" << port;
        }
      }
    }
  }
}

void
Node::_processPingResponse(
    const Message &msg, size_t size, PingRequest *req, const QHostAddress &addr, uint16_t port)
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
    qDebug() << "Still bootstrapping: Search for myown.";
    findNode(_self);
  }
}

void
Node::_processFindNodeResponse(
    const Message &msg, size_t size, FindNodeRequest *req, const QHostAddress &addr, uint16_t port)
{
  qDebug() << "Received FindNode response from " << addr << ":" << port;
  // payload length must be a multiple of triple length
  if ( 0 != ((size-DHT_HASH_SIZE-1)%DHT_TRIPLE_SIZE) ) {
    qDebug() << "Received a malformed FIND_NODE response from"
             << addr << ":" << port;
  } else {
    // unpack and update query
    size_t Ntriple = (size-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE;
    for (size_t i=0; i<Ntriple; i++) {
      Identifier id(msg.payload.result.triples[i].id);
      NodeItem item(id, QHostAddress(ntohl(msg.payload.result.triples[i].ip)),
                    ntohs(msg.payload.result.triples[i].port));
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
      emit nodeNotFound(req->query()->id());
    }
    // delete query
    delete req->query();
    // done.
    return;
  }

  // Send next request
  sendFindNode(next, req->query());
}

void
Node::_processFindValueResponse(
    const Message &msg, size_t size, FindValueRequest *req, const QHostAddress &addr, uint16_t port)
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
Node::_processPingRequest(
    const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  qDebug() << "Received Ping request from" << addr << ":" << port;
  // simply assemble a pong response including my own ID
  Message resp;
  memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
  memcpy(resp.payload.ping.id, _self.data(), DHT_HASH_SIZE);
  resp.payload.ping.type = Message::PING;
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
Node::_processFindNodeRequest(
    const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  QList<NodeItem> best;
  _buckets.getNearest(msg.payload.find_node.id, best);

  Message resp;
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

void
Node::_processFindValueRequest(
    const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
{
  Message resp;

  if (_announcements.contains(msg.payload.find_value.id)) {
    QHash<Identifier, AnnouncementItem> &owners
        = _announcements[msg.payload.find_value.id];
    // Assemble response
    memcpy(resp.cookie, msg.cookie, DHT_HASH_SIZE);
    resp.payload.result.success = 1;
    QHash<Identifier, AnnouncementItem>::iterator item = owners.begin();
    for (int i = 0; (item!=owners.end()) && (i<DHT_K); item++, i++) {
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
Node::_processAnnounceRequest(
    const Message &msg, size_t size, const QHostAddress &addr, uint16_t port)
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
Node::_onCheckRequestTimeout() {
  QList<Request *> deadRequests;
  QHash<Identifier, Request *>::iterator item = _pendingRequests.begin();
  for (; item != _pendingRequests.end(); item++) {
    if ((*item)->olderThan(2)) { deadRequests.append(*item); }
  }

  // Process dead requests, dispatch by type
  QList<Request *>::iterator req = deadRequests.begin();
  for (; req != deadRequests.end(); req++) {
    _pendingRequests.remove((*req)->cookie());
    if (Message::PING == (*req)->type()) {
      // Just ignore
      delete *req;
    } else if (Message::FIND_NODE == (*req)->type()) {
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
          emit nodeNotFound(query->id());
        }
        // delete query
        delete query;
      }
      // Send next request
      sendFindNode(next, query);
    } else if (Message::FIND_VALUE == (*req)->type()) {
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
      sendFindValue(next, query);
    }
  }
}

void
Node::_onCheckNodeTimeout() {
  qDebug() << "Refresh buckets.";
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
Node::_onCheckAnnouncementTimeout() {
  qDebug() << "Refresh announcements.";
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
