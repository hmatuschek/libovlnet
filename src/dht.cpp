#include "dht.h"

/* ******************************************************************************************** *
 * Implementation of Identifier
 * ******************************************************************************************** */
Identifier::Identifier()
  : QByteArray()
{
  // pass...
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
 * Implementation of Bucket::Item
 * ******************************************************************************************** */
Bucket::Item::Item()
  : _id(), _prefix(0), _addr(), _port(0), _lastSeen()
{
  // pass...
}

Bucket::Item::Item(const Identifier &id, const QHostAddress &addr, uint16_t port, size_t prefix)
  : _id(id), _prefix(prefix), _addr(addr), _port(port), _lastSeen(QDateTime::currentDateTime())
{
  // pass...
}

Bucket::Item::Item(const Item &other)
  : _id(other._id), _prefix(other._prefix), _addr(other._addr), _port(other._port),
    _lastSeen(other._lastSeen)
{
  // pass...
}

Bucket::Item &
Bucket::Item::operator =(const Item &other) {
  _id = other._id;
  _prefix = other._prefix;
  _addr = other._addr;
  _port = other._port;
  _lastSeen = other._lastSeen;
  return *this;
}

const Identifier &
Bucket::Item::id() const {
  return _id;
}

size_t
Bucket::Item::prefix() const {
  return _prefix;
}
const QHostAddress &
Bucket::Item::addr() const {
  return _addr;
}

uint16_t
Bucket::Item::port() const {
  return _port;
}

const QDateTime &
Bucket::Item::lastSeen() const {
  return _lastSeen;
}


/* ******************************************************************************************** *
 * Implementation of Bucket
 * ******************************************************************************************** */
Bucket::Bucket(size_t size)
  : _maxSize(size), _prefix(0), _triples()
{
  // pass...
}

Bucket::Bucket(const Bucket &other)
  : _maxSize(other._maxSize), _prefix(other._prefix), _triples(other._triples)
{
  // pass...
}

bool
Bucket::full() const {
  return _maxSize==_triples.size();
}

bool
Bucket::contains(const Identifier &id) {
  return _triples.contains(id);
}

void
Bucket::add(const Item &item) {
  if (contains(item.id())) {
    _history.removeOne(item.id());
  } else if (full()) {
    Identifier oldest = _history.front();
    _history.pop_front(); _triples.remove(oldest);
  }
  // add item as the newest
  _history.append(item.id());
  // Add or update item
  _triples.insert(item.id(), item);
}

size_t
Bucket::prefix() const {
  return _prefix;
}

void
Bucket::split(Bucket &newBucket) {
  QList<Identifier>::iterator id = _history.begin();
  for (; id != _history.end(); id++) {
    Item &item = _triples[*id];
    if (item.prefix() > _prefix) {
      newBucket.add(item);
    }
  }
  id = newBucket._history.begin();
  for (; id != newBucket._history.begin(); id++) {
    _history.removeOne(*id); _triples.remove(*id);
  }
}


/* ******************************************************************************************** *
 * Implementation of Buckets
 * ******************************************************************************************** */
Buckets::Buckets()
  : _buckets()
{
  _buckets.reserve(8*DHT_HASH_SIZE);
}

void
Buckets::add(const Bucket::Item &item) {
  // If there are no buckets -> create one.
  if (0 == _buckets.size()) {
    _buckets.append(Bucket());
    _buckets[0].add(item);
    return;
  }
  // Find the bucket, the item belongs to
  size_t idx = index(item);
  Bucket &bucket = _buckets[idx];

  if (bucket.contains(item.id())) {
    // If the bucket contains the item already -> update
    bucket.add(item);
  } else if (bucket.full() && ((_buckets.size()-1)  == idx)) {
    // If the bucket is full and I could create one -> do it
    Bucket newBucket;
    bucket.split(newBucket);
    _buckets.append(newBucket);
    // Repeat step if needed
    this->add(item);
  } else {
    // If I cannot create a new Bucket or if the bucket is not full
    bucket.add(item);
  }
}

size_t
Buckets::index(const Bucket::Item &item) const {
  if (2 > _buckets.size()) { return 0; }
  for (size_t i=0; i<(_buckets.size()-1); i++) {
    if (_buckets[i].prefix() == item.prefix()) { return i; }
    if (_buckets[i+1].prefix() > item.prefix()) { return i; }
  }
  return _buckets.size()-1;
}


/* ******************************************************************************************** *
 * Implementation of RequestItem etc.
 * ******************************************************************************************** */
RequestItem::RequestItem(Message::Type type)
  : _type(type), _timeStamp(QDateTime::currentDateTime())
{
  // pass...
}

PingRequestItem::PingRequestItem(const Identifier &id)
  : RequestItem(Message::PING), _id(id)
{
  // pass...
}


/* ******************************************************************************************** *
 * Implementation of Node
 * ******************************************************************************************** */
Node::Node(const Identifier &id, const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _id(id), _socket()
{
  if (!_socket.bind(addr, port)) {
    qDebug() << "Cannot bind to port" << addr << ":" << port;
    return;
  }

  QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
}

Node::~Node() {
  // pass...
}

void
Node::_onReadyRead() {
  while (_socket.hasPendingDatagrams()) {
    // check datagram size
    if ( (_socket.pendingDatagramSize() > DHT_MAX_MESSAGE_SIZE) ||
         (_socket.pendingDatagramSize() < DHT_MIN_MESSAGE_SIZE)) {
      // Cannot be a vaild message -> drop silently
      _socket.readDatagram(0,0);
    }

    // Read message
    Message msg; QHostAddress addr; uint16_t port;
    int64_t size = _socket.readDatagram((char *) &msg, DHT_MAX_MESSAGE_SIZE, &addr, &port);
    if (0 > size) { continue; }

    Identifier cookie(msg.cookie);

    if (_pendingRequests.contains(cookie)) {
      // Message is a response -> dispatch by type from table
      RequestItem *item = _pendingRequests[cookie];
      if (Message::PING == item->type()) {
        // Response to a ping request -> update buckets.
        PingRequestItem *pitem = reinterpret_cast<PingRequestItem *>(item);
        _buckets.add(Bucket::Item(pitem->identifier(), addr, port,
                                  (pitem->identifier()-_id).leadingBit()));
      } else if (Message::FIND_NODE == item->type()) {
        // Response to a find node request

      }
    } else {
      // Message is likely a request
    }
  }
}


