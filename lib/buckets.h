#ifndef BUCKETS_H
#define BUCKETS_H

#include "dht_config.h"
#include "logger.h"

#include <QByteArray>
#include <QList>
#include <QHash>
#include <QDebug>
#include <QHostAddress>
#include <QDateTime>

#include <inttypes.h>


// Forward declaration
class Identifier;

/** The distance between two identifiers. */
class Distance : public QByteArray
{
public:
  /** Computes the distance between the identifiers @c a and @c b. */
  Distance(const Identifier &a, const Identifier &b);
  /** Copy constructor. */
  Distance(const Distance &distance);
  /** Returns the bit at index @c idx of the distance. Bit 0 is the MSB. */
  bool bit(size_t idx) const;
  /** Returns the index of the leading non-zero bit of the distance.*/
  size_t leadingBit() const;
};


/** Represents an identifier in the DHT. */
class Identifier : public QByteArray
{
public:
  /** Creates a new random identifier. */
  Identifier();
  /** Constructor. */
  Identifier(const char *id);
  /** Constructor. */
  Identifier(const QByteArray &id);
  /** Copy constructor. */
  Identifier(const Identifier &other);

  /** Assignment. */
  Identifier &operator=(const Identifier &other);
  /** Comparison. */
  bool operator==(const Identifier &other) const;
  /** Computes the distance. */
  Distance operator-(const Identifier &other) const;
};

inline QDebug &operator<<(QDebug &stream, const Identifier &id) {
  stream << id.toHex();
  return stream;
}

inline QTextStream &operator<<(QTextStream &stream, const Identifier &id) {
  stream << QString::fromLocal8Bit(id.toHex());
  return stream;
}


/** Represents a peer (IP address + port) in the network. */
class PeerItem
{
public:
  PeerItem();
  PeerItem(const QHostAddress &addr, uint16_t port);
  PeerItem(const PeerItem &other);

  PeerItem &operator=(const PeerItem &other);

  const QHostAddress &addr() const;
  uint16_t port() const;

protected:
  QHostAddress _addr;
  uint16_t     _port;
};


/** Represents a node (ID + IP address + port) in the network. */
class NodeItem: public PeerItem
{
public:
  NodeItem();
  NodeItem(const Identifier &id, const QHostAddress &addr, uint16_t port);
  NodeItem(const Identifier &id, const PeerItem &peer);
  NodeItem(const NodeItem &other);

  NodeItem &operator=(const NodeItem &other);

  const Identifier &id() const;

protected:
  Identifier _id;
};


class AnnouncementItem: public PeerItem
{
public:
  AnnouncementItem();
  AnnouncementItem(const QHostAddress &addr, uint16_t port);
  AnnouncementItem(const AnnouncementItem &other);

  AnnouncementItem &operator =(const AnnouncementItem &other);

  bool olderThan(size_t seconds);

protected:
  QDateTime _timestamp;
};


/** Represents a single k-bucket. */
class Bucket
{
public:
  /** An element of the @c Bucket. */
  class Item
  {
  public:
    /** Empty constructor. */
    Item();
    /** Constructor from identifier, address, port and prefix. */
    Item(const QHostAddress &addr, uint16_t port, size_t prefix);
    /** Copy constructor. */
    Item(const Item &other);
    /** Assignment operator. */
    Item &operator=(const Item &other);

    /** Returns the precomputed @c prefix of the item w.r.t. the ID of the node. */
    size_t prefix() const;
    const PeerItem &peer() const;
    /** The address of the item. */
    const QHostAddress &addr() const;
    /** The port of the item. */
    uint16_t port() const;
    /** The time of the item last seen. */
    const QDateTime &lastSeen() const;
    /** Returns true if the entry is older than the specified seconds. */
    inline size_t olderThan(size_t seconds) const {
      return (_lastSeen.addSecs(seconds) < QDateTime::currentDateTime());
    }

  protected:
    /** The prefix -- index of the leading bit of the difference between this identifier and the
     * identifier of the node. */
    size_t       _prefix;
    PeerItem     _peer;
    /** The time, the item was last seen. */
    QDateTime    _lastSeen;
  };

public:
  /** Constructor. */
  Bucket(const Identifier &self);
  /** Copy constructor. */
  Bucket(const Bucket &other);

  void getNearest(const Identifier &id, QList<NodeItem> &best) const;
  void getOlderThan(size_t age, QList<NodeItem> &nodes) const;
  void removeOlderThan(size_t age);

  /** Returns @c true if the bucket is full. */
  bool full() const;
  /** Returns the number of nodes held in the bucket. */
  size_t numNodes() const;
  /** Returns the list of all nodes held in the bucket. */
  void nodes(QList<NodeItem> &lst) const;
  /** Returns @c true if the bucket contains the given identifier. */
  bool contains(const Identifier &id) const;
  /** Add or updates an item. */
  void add(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** The prefix of the bucket. */
  size_t prefix() const;

  /** Splits the bucket at its prefix. Means all item with a higher prefix (smaller distance)
   * than the prefix of this bucket are moved to the new one. */
  void split(Bucket &newBucket);

protected:
  /** Adds an item. */
  void add(const Identifier &id, const Item &item);

protected:
  Identifier _self;
  /** The maximal bucket size. */
  size_t _maxSize;
  /** The prefix of the bucket. */
  size_t _prefix;
  /** Item table. */
  QHash<Identifier, Item> _triples;
};


/** A ordered list of buckets. */
class Buckets
{
public:
  /** Constructor. */
  Buckets(const Identifier &self);

  bool empty() const;
  bool contains(const Identifier &id) const;
  /** Returns the number of nodes in the buckets. */
  size_t numNodes() const;
  /** Returns the list of all nodes in the buckets. */
  void nodes(QList<NodeItem> &lst) const;
  /** Adds or updates an item. */
  void add(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** Collects the nearest known nodes. */
  void getNearest(const Identifier &id, QList<NodeItem> &best) const;
  /** Collects all nodes that are "older" than the specified age (in seconds). */
  void getOlderThan(size_t seconds, QList<NodeItem> &nodes) const;
  /** Removes all nodes that are "older" than the specified age (in seconds). */
  void removeOlderThan(size_t seconds);

protected:
  /** Returns the bucket index, an item should be searched for. */
  QList<Bucket>::iterator index(const Identifier &id);

protected:
  Identifier _self;
  /** The bucket list. */
  QList<Bucket> _buckets;
};


#endif // BUCKETS_H
