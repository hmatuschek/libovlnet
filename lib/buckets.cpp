#include "buckets.h"
#include <inttypes.h>


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

